#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run only WS73 Wi-Fi byte codec tests in a hardware-free x86-64 VM."""
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
CASE_ORDER = ['ws73_frw_' + name for name in ('request_bytes', 'request_limits', 'reply_bytes',
              'reply_truncation', 'reply_metadata', 'reply_bounds', 'zero_and_maximum', 'null_args')]
CASES = set(CASE_ORDER)


def validate_raw(raw):
    lines = raw.splitlines()
    expected = [f'    ok {i} {name}' for i, name in enumerate(CASE_ORDER, 1)]
    actual = [line for line in lines if re.match(r'^\s*(?:not )?ok \d+ ws73_frw_', line)]
    if actual != expected:
        raise ValueError('raw cases missing, duplicated, skipped, failed or reordered')
    for marker in ('    # Subtest: ws73-wifi-frw', '    1..8',
                   '# ws73-wifi-frw: pass:8 fail:0 skip:0 total:8',
                   '# Totals: pass:8 fail:0 skip:0 total:8', 'ok 1 ws73-wifi-frw',
                   'reboot: machine restart'):
        if lines.count(marker) != 1:
            raise ValueError('missing/repeated raw marker: ' + marker)


def validate(result, raw, config):
    groups = result.get('sub_groups', [])
    if len(groups) != 1 or groups[0].get('name') != 'ws73-wifi-frw':
        raise ValueError('missing/extra Wi-Fi codec suite')
    cases = groups[0].get('test_cases', [])
    if (len(cases) != len(CASES) or {c['name'] for c in cases} != CASES or
            any(c['status'] != 'PASS' for c in cases)):
        raise ValueError('missing, repeated, skipped or failed codec test')
    required = {line for line in (HERE / 'kunitconfig').read_text().splitlines() if line}
    if not required <= set(config.splitlines()):
        raise ValueError('missing requested configuration')
    COMMON.sanitizer_summary(raw, config)
    validate_raw(raw)
    return {'suite': 'ws73-wifi-frw', 'cases': len(cases)}


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
                    'scope': 'candidate FRW codec only; no adapter, transport, firmware or hardware qualification'}
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
            required += ['drivers/net/wireless/hisilicon/ws73/' + name for name in ('frw.o', 'frw-test.o')]
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
