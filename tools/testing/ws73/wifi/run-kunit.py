#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run only WS73 Wi-Fi codec and transaction tests in a hardware-free x86-64 VM."""
import datetime
import fcntl
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[3]
WORK = SOURCE.parent / '.dev/ws73-upstream'
SPEC = importlib.util.spec_from_file_location('ws73_verification', HERE.parent / 'run-kunit.py')
COMMON = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMMON)
SUITES = {
    'ws73-wifi-frw': ['ws73_frw_' + name for name in (
        'request_bytes', 'request_limits', 'reply_bytes', 'reply_truncation',
        'reply_metadata', 'reply_bounds', 'zero_and_maximum', 'null_args')],
    'ws73-wifi-frw-transaction': ['ws73_frw_transaction_' + name for name in (
        'success', 'failures', 'deadline', 'timeout', 'exhaustion', 'validation',
        'independent_rx', 'shutdown_drain', 'inactive')],
}


def validate_raw(raw):
    lines = raw.splitlines()
    expected = [f'    ok {i} {name}' for names in SUITES.values()
                for i, name in enumerate(names, 1)]
    actual = [line for line in lines if re.match(r'^\s*(?:not )?ok \d+ ws73_frw_', line)]
    if actual != expected:
        raise ValueError('raw cases missing, duplicated, skipped, failed or reordered')
    markers = ['1..2', 'reboot: machine restart']
    for i, (suite, names) in enumerate(SUITES.items(), 1):
        count = len(names)
        markers += [f'    # Subtest: {suite}', f'    1..{count}',
                    f'# {suite}: pass:{count} fail:0 skip:0 total:{count}',
                    f'# Totals: pass:{count} fail:0 skip:0 total:{count}', f'ok {i} {suite}']
    positions = []
    for marker in markers:
        if lines.count(marker) != 1:
            raise ValueError('missing/repeated raw marker: ' + marker)
    for i, (suite, names) in enumerate(SUITES.items(), 1):
        positions += [lines.index(f'    # Subtest: {suite}')]
        positions += [lines.index(f'    ok {n} {name}') for n, name in enumerate(names, 1)]
        positions += [lines.index(f'ok {i} {suite}')]
    positions += [lines.index('reboot: machine restart')]
    if positions != sorted(positions):
        raise ValueError('raw suite/case/termination order is inconsistent')


def validate(result, raw, config):
    groups = result.get('sub_groups', [])
    if [g.get('name') for g in groups] != list(SUITES):
        raise ValueError('missing/extra/reordered Wi-Fi suite')
    for group in groups:
        cases = group.get('test_cases', [])
        names = SUITES[group['name']]
        if ([c['name'] for c in cases] != names or
                any(c['status'] != 'PASS' for c in cases)):
            raise ValueError('missing, repeated, skipped or failed test')
    required = {line for line in (HERE / 'kunitconfig').read_text().splitlines() if line}
    if not required <= set(config.splitlines()):
        raise ValueError('missing requested configuration')
    COMMON.sanitizer_summary(raw, config)
    validate_raw(raw)
    return {'suites': {name: len(cases) for name, cases in SUITES.items()},
            'cases': sum(len(cases) for cases in SUITES.values())}


def main():
    if not __debug__:
        raise RuntimeError('run without Python optimization')
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'build.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if COMMON.git('status', '--porcelain').strip():
            raise RuntimeError('commit clean source before qualification')
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        record = WORK / 'records' / ('wifi-frw-' + stamp)
        record.mkdir(parents=True)
        print(record, flush=True)
        build = WORK / 'build-wifi-kunit'
        shutil.copyfile(HERE / 'kunitconfig', record / 'kunitconfig')
        shutil.copyfile(__file__, record / 'run-kunit.py')
        shutil.copyfile(HERE.parent / 'run-kunit.py', record / 'common-run-kunit.py')
        manifest = {'status': 'INCOMPLETE', 'source': COMMON.source_identity(record),
                    'tools': COMMON.tools_identity(), 'artifacts': {},
                    'scope': 'candidate FRW codec and bounded transaction only; no adapter, transport, firmware or hardware qualification'}
        command = [sys.executable, 'tools/testing/kunit/kunit.py', 'run', '--arch=x86_64',
                   f'--build_dir={build}', f'--kunitconfig={record / "kunitconfig"}', '--jobs=4',
                   '--make_options=LLVM=1', '--make_options=W=1', '--timeout=120',
                   '--kernel_args=panic_on_warn=1', '--qemu_args=-smp 2 -nic none',
                   f'--json={record / "results.json"}', '*ws73*']
        manifest['command'] = command

        def save():
            (record / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

        save()
        try:
            with (record / 'run.log').open('w') as log:
                result = subprocess.run(command, cwd=SOURCE, stdout=log, stderr=subprocess.STDOUT,
                                        env=dict(os.environ, CCACHE_DISABLE='1', PYTHONUNBUFFERED='1'))
            manifest['exit_code'] = result.returncode
            required = ['.config', 'arch/x86/boot/bzImage', 'vmlinux', 'test.log']
            required += ['drivers/net/wireless/hisilicon/ws73/' + name for name in ('frw.o', 'frw-test.o', 'frw-transaction.o', 'frw-transaction-test.o')]
            for name in required:
                target = record / 'artifacts' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(build / name, target)
                manifest['artifacts'][name] = COMMON.digest(target)
            after = record / 'after'
            after.mkdir()
            manifest['source_after'] = COMMON.source_identity(after)
            if result.returncode or manifest['source'] != manifest['source_after']:
                raise RuntimeError('failed execution or source changed')
            raw = (record / 'artifacts/test.log').read_text()
            config = (record / 'artifacts/.config').read_text()
            parsed = json.loads((record / 'results.json').read_text())
            manifest['observed'] = validate(parsed, raw, config)
            manifest['status'] = 'PASS'
        except Exception as exc:
            manifest['status'] = 'FAIL'
            manifest['error'] = repr(exc)
            raise
        finally:
            manifest['evidence_sha256'] = {
                str(p.relative_to(record)): COMMON.digest(p) for p in record.rglob('*')
                if p.is_file() and p != record / 'manifest.json'}
            save()
        print('PASS', flush=True)


if __name__ == '__main__':
    main()
