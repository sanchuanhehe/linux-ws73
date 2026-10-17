#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check the finite USB stop-producer model; never execute a kernel on host."""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[3]
WORK = SOURCE.parent / '.dev/ws73-upstream'
MESSAGE = re.compile(r'@!@!@STARTMSG (\d+):(\d+) @!@!@\n(.*?)@!@!@ENDMSG \1 @!@!@', re.S)
INPUTS = ['drivers/misc/ws73/' + f for f in
          ('usb.c', 'usb-io.c', 'usb-owner.h', 'ble.c', 'ble-link.c', 'ble-link.h')]
INPUTS += ['drivers/usb/core/urb.c', 'drivers/usb/core/hcd.c']
INPUTS += ['tools/testing/ws73/formal/' + f for f in
           ('USBStop.tla', 'USBStop-current.cfg', 'USBStop-old-order.cfg',
            'USBStop.README', 'run-tlc.py')]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', '-C', str(SOURCE), *args], text=True).strip()


def validate(raw, returncode, old=False):
    messages = [(int(m[1]), int(m[2]), m[3].strip()) for m in MESSAGE.finditer(raw)]
    residue = MESSAGE.sub('', raw)
    if '@!@!@' in residue or re.search(r'Error:|Exception|WARNING|unsupported', raw):
        raise ValueError('truncated message or tool diagnostic')
    groups = {}
    for code, level, body in messages:
        groups.setdefault(code, []).append((level, body))

    def one(code):
        found = groups.get(code, [])
        if len(found) != 1:
            raise ValueError(f'missing/duplicate TLC message {code}')
        return found[0][1]

    if one(2262) != 'TLC2 Version 2.19 of 08 August 2024 (rev: 5a47802)':
        raise ValueError('unreviewed TLC version')
    if not one(2186).startswith('Finished in '):
        raise ValueError('missing completion')
    if not re.fullmatch(r'Finished computing initial states: 1 distinct state generated at .+\.', one(2190)):
        raise ValueError('wrong initial state inventory')
    stats = re.fullmatch(r'(\d+) states generated, (\d+) distinct states found, (\d+) states left on queue\.', one(2199))
    if not stats:
        raise ValueError('missing complete statistics')
    generated, distinct, queue = map(int, stats.groups())
    if not generated >= distinct > 1:
        raise ValueError('empty state exploration')
    errors = [(code, body) for code, level, body in messages if level in (1, 2, 3)]
    traces = groups.get(2217, [])
    if not old:
        if returncode or errors or traces or queue:
            raise ValueError('incomplete or failed state exploration')
        if not one(2193).startswith('Model checking completed. No error has been found.\n'):
            raise ValueError('missing successful exploration')
    else:
        if returncode != 12 or errors != [(2110, 'Invariant NoUseAfterFree is violated.'),
                                         (2121, 'The behavior up to this point is:')]:
            raise ValueError('missing intended invariant violation')
        if 2193 in groups or len(traces) < 2:
            raise ValueError('missing counterexample or contradictory success')
        for i, (level, body) in enumerate(traces, 1):
            if level != 4 or not body.startswith(f'{i}: <'):
                raise ValueError('noncontiguous trace')
        bodies = [body for _, body in traces]
        names = [body.split('\n', 1)[0] for body in bodies]
        # Require the concrete delayed-backend schedule, not just any failure.
        def step(name):
            indices = [i for i, line in enumerate(names) if f'<{name} ' in line]
            if len(indices) != 1:
                raise ValueError('missing/duplicate counterexample step ' + name)
            return indices[0]
        claim, close, cancel, enqueue, remove, free = map(
            step, ('ClaimFault', 'Close', 'BeforeChild', 'QueueStop', 'RemoveChild', 'Free'))
        if not claim < close < cancel < enqueue < remove < free == len(bodies) - 1:
            raise ValueError('counterexample does not match late backend scheduling')
        if ('backend |-> "claimed"' not in bodies[cancel] or
                '/\\ stop = "idle"' not in bodies[cancel] or
                '/\\ pc = "dead"' not in bodies[-1] or
                '/\\ stop = "queued"' not in bodies[-1] or
                'backend |-> "done"' not in bodies[-1]):
            raise ValueError('counterexample state mismatch')
    return {'generated': generated, 'distinct': distinct, 'queue': queue,
            'trace_states': len(traces), 'status': 'EXPECTED_COUNTEREXAMPLE' if old else 'FINITE_MODEL_PASS'}


def controls(good, bad):
    cases = [('missing finish', good[:good.index('@!@!@STARTMSG 2186')], 0, False),
             ('failed process', good, 1, False), ('wrong old exit', bad, 0, True),
             ('swapped positive', bad, 12, False), ('swapped negative', good, 0, True),
             ('truncated marker', good[:-10], 0, False),
             ('wrong invariant', bad.replace('Invariant NoUseAfterFree', 'Invariant TypeOK'), 12, True),
             ('lost backend evidence', bad.replace('backend |-> "claimed"', 'backend |-> "active"'), 12, True)]
    for code, label in ((2193, 'success'), (2199, 'statistics'), (2190, 'initial states')):
        block = next(m.group(0) for m in MESSAGE.finditer(good) if int(m[1]) == code)
        cases += [('missing ' + label, good.replace(block, ''), 0, False),
                  ('duplicate ' + label, good + '\n' + block, 0, False)]
    cases.append(('warning', good + '\n@!@!@STARTMSG 9999:3 @!@!@\nwarning\n@!@!@ENDMSG 9999 @!@!@\n', 0, False))
    for name, raw, rc, old in cases:
        try:
            validate(raw, rc, old)
        except ValueError:
            continue
        raise ValueError('supervisor accepted ' + name)
    return [name for name, *_ in cases]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True, help='TLC 2.19 (rev 5a47802) tla2tools.jar')
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'build.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if git('status', '--porcelain'):
            raise RuntimeError('commit clean sources before qualification')
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        record = WORK / 'records' / ('tlc-usb-stop-' + stamp)
        record.mkdir(parents=True)
        print(record, flush=True)
        manifest = {'status': 'NOT_PROVEN', 'source': git('rev-parse', 'HEAD'), 'runs': [], 'artifacts': {}}

        def save():
            manifest['artifacts'] = {str(p.relative_to(record)): sha(p) for p in sorted(record.rglob('*'))
                                     if p.is_file() and p != record / 'manifest.json' and 'states' not in p.parts}
            (record / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

        try:
            for name in INPUTS:
                target = record / 'inputs' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(SOURCE / name, target)
            # Drift guard only; this is not an automatic proof of C/model refinement.
            src = (SOURCE / 'drivers/misc/ws73/usb.c').read_text()
            unbind = src.split('void ws73_usb_unbind(', 1)[1].split('EXPORT_SYMBOL', 1)[0]
            if not (unbind.index('cancel_work_sync(&ws->initialize)') <
                    unbind.index('ws73_ble_link_remove(&ws->ble)') <
                    unbind.index('cancel_work_sync(&ws->stop)') < unbind.index('kfree(ws)')):
                raise RuntimeError('production teardown order no longer matches current model')
            jar = record / 'tla2tools.jar'
            shutil.copyfile(args.jar.resolve(strict=True), jar)
            java = Path(shutil.which('java')).resolve()
            manifest['java'] = {'path': str(java), 'sha256': sha(java), 'version': subprocess.check_output(
                [str(java), '-version'], stderr=subprocess.STDOUT, text=True)}
            for mode in ('current', 'old-order'):
                folder = record / mode
                folder.mkdir()
                shutil.copyfile(HERE / 'USBStop.tla', folder / 'USBStop.tla')
                shutil.copyfile(HERE / f'USBStop-{mode}.cfg', folder / 'USBStop.cfg')
                command = [str(java), '-XX:ActiveProcessorCount=1', '-XX:+UseParallelGC', '-Xmx512m',
                           '-Djava.rmi.server.hostname=127.0.0.1', '-cp', str(jar), 'tlc2.TLC',
                           '-workers', '1', '-tool', '-seed', '1', '-fp', '0', '-config', 'USBStop.cfg',
                           '-metadir', str(folder / 'states'), '-dump', str(folder / 'reachable'), 'USBStop.tla']
                run = {'mode': mode, 'command': command, 'status': 'NOT_PROVEN'}
                manifest['runs'].append(run)
                save()
                with (folder / 'stdout.log').open('w') as out, (folder / 'stderr.log').open('w') as err:
                    proc = subprocess.Popen(command, cwd=folder, stdout=out, stderr=err, start_new_session=True)
                    try:
                        run['returncode'] = proc.wait(timeout=60)
                    except subprocess.TimeoutExpired:
                        os.killpg(proc.pid, signal.SIGKILL)
                        proc.wait()
                        run['timeout'] = True
                        raise RuntimeError('TLC deadline exceeded; not proven')
                if (folder / 'stderr.log').read_text():
                    raise RuntimeError('unexpected TLC stderr')
                result = validate((folder / 'stdout.log').read_text(), run['returncode'], mode == 'old-order')
                if mode == 'current':
                    dump = (folder / 'reachable.dump').read_text()
                    states = re.split(r'^State \d+:.*\n', dump, flags=re.M)[1:]
                    if len(states) != result['distinct'] or not any('/\\ pc = "dead"' in s for s in states):
                        raise RuntimeError('missing complete reachable state dump or terminal witness')
                    result['terminal_states'] = sum('/\\ pc = "dead"' in s for s in states)
                run.update(result)
                save()
            manifest['rejected_controls'] = controls((record / 'current/stdout.log').read_text(),
                                                     (record / 'old-order/stdout.log').read_text())
            manifest['status'] = 'FINITE_MODEL_PASS'
        except Exception as exc:
            manifest['error'] = repr(exc)
            raise
        finally:
            save()
        print(manifest['status'], flush=True)


if __name__ == '__main__':
    main()
