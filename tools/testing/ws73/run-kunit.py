#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run the WS73 codec in a private, hardware-free VM and retain provenance."""

import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


SOURCE = Path(__file__).resolve().parents[3]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(*args):
    return subprocess.check_output(["git", "-C", str(SOURCE), *args])


def source_identity(record):
    patch = git("diff", "HEAD", "--binary")
    untracked = git("ls-files", "--others", "--exclude-standard", "-z")
    files = {}
    for name in untracked.decode().split("\0"):
        if name:
            files[name] = digest(SOURCE / name)
    (record / "tracked.patch").write_bytes(patch)
    return {
        "head": git("rev-parse", "HEAD").decode().strip(),
        "status": git("status", "--porcelain=v1").decode(),
        "tracked_patch_sha256": hashlib.sha256(patch).hexdigest(),
        "untracked_sha256": files,
    }


def tools_identity():
    result = {}
    for name in ("python3", "make", "clang", "ld.lld", "qemu-system-x86_64"):
        path = shutil.which(name)
        if not path:
            raise RuntimeError(f"missing tool: {name}")
        version = subprocess.check_output([path, "--version"], text=True)
        # Include the real compiler when PATH first resolves to a ccache shim.
        candidates = {str((Path(entry) / name).resolve())
                      for entry in os.get_exec_path()
                      if (Path(entry) / name).is_file()}
        result[name] = {"path": path, "sha256": digest(Path(path)), "version": version,
                        "path_candidates_sha256": {p: digest(Path(p)) for p in sorted(candidates)}}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4,
                        help="reserved share of the whole-machine budget (1..8)")
    args = parser.parse_args()
    if not 1 <= args.jobs <= 8:
        parser.error("reserve 1..8 jobs after checking other builds and hardware VMs")

    work = SOURCE.parent / ".dev/ws73-upstream"
    work.mkdir(parents=True, exist_ok=True)
    with (work / "build.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        record = work / "records" / stamp
        record.mkdir(parents=True)
        build = work / "build-kunit"
        before = source_identity(record)
        manifest = {
            "scope": "synthetic codec tests in QEMU; no USB, firmware, HCI registration or peer",
            "source": before,
            "tools": tools_identity(),
            "runner_sha256": digest(Path(__file__)),
            "jobs": args.jobs,
            "status": "INCOMPLETE",
        }
        qboot = Path("/usr/share/qemu/qboot.rom")
        if qboot.is_file():
            manifest["qboot"] = {"path": str(qboot), "sha256": digest(qboot)}
        cmd = [sys.executable, "tools/testing/kunit/kunit.py", "run",
               "--arch=x86_64", f"--build_dir={build}",
               f"--kunitconfig={SOURCE / 'tools/testing/ws73/kunitconfig'}",
               f"--jobs={args.jobs}", "--make_options=LLVM=1", "--make_options=W=1",
               "--timeout=120", "--kernel_args=panic_on_warn=1",
               "--qemu_args=-smp 2 -nic none "
               f"-qmp unix:{work / 'kunit.qmp'},server=on,wait=off",
               f"--json={record / 'results.json'}", "*ws73*"]
        manifest["command"] = cmd
        manifest_path = record / "manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        print(record, flush=True)
        env = dict(os.environ, CCACHE_DISABLE="1", PYTHONUNBUFFERED="1")
        with (record / "run.log").open("w") as log:
            run = subprocess.run(cmd, cwd=SOURCE, env=env, stdout=log,
                                 stderr=subprocess.STDOUT, check=False)
        manifest["exit_code"] = run.returncode
        # Preserve the pre-build patch, then compare the post-build identity.
        after_dir = record / "after"
        after_dir.mkdir()
        manifest["source_after"] = source_identity(after_dir)
        manifest["artifacts"] = {}
        for name in (".config", "arch/x86/boot/bzImage", "vmlinux", "test.log",
                     "drivers/bluetooth/btws73_proto.o", "drivers/bluetooth/btws73_test.o",
                     "drivers/misc/ws73/hcc.o", "drivers/misc/ws73/firmware.o",
                     "drivers/misc/ws73/boot.o", "drivers/misc/ws73/test.o"):
            path = build / name
            if path.is_file():
                dest = record / "artifacts" / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
                manifest["artifacts"][name] = digest(dest)
        passed = False
        result_file = record / "results.json"
        if result_file.exists():
            result = json.loads(result_file.read_text())
            groups = result.get("sub_groups", [])
            expected = {
                "btws73_rx_valid", "btws73_rx_truncated", "btws73_rx_identity",
                "btws73_rx_lengths", "btws73_rx_maximum", "btws73_hci_types",
                "btws73_hci_lengths", "btws73_tx_bytes", "btws73_tx_limits",
                "btws73_null_args",
                "btws73_chip_query", "btws73_chip_reply", "btws73_chip_invalid",
            }
            expected_groups = {
                "btws73-proto": expected,
                "ws73-fw-boot": {
                    "ws73_profiles_test", "ws73_file_test", "ws73_boot_sequence_test",
                    "ws73_boot_faults_test", "ws73_boot_admission_test",
                },
            }
            passed = len(groups) == len(expected_groups)
            seen = set()
            for group in groups:
                cases = group.get("test_cases", [])
                names = expected_groups.get(group["name"], set())
                passed = (passed and bool(names) and group["name"] not in seen
                          and len(cases) == len(names)
                          and {case["name"] for case in cases} == names
                          and all(case["status"] == "PASS" for case in cases))
                seen.add(group["name"])
            passed = passed and seen == expected_groups.keys()
        required = {".config", "arch/x86/boot/bzImage", "vmlinux", "test.log",
                    "drivers/bluetooth/btws73_proto.o", "drivers/bluetooth/btws73_test.o",
                    "drivers/misc/ws73/hcc.o", "drivers/misc/ws73/firmware.o",
                    "drivers/misc/ws73/boot.o", "drivers/misc/ws73/test.o"}
        passed = (passed and run.returncode == 0 and before == manifest["source_after"]
                  and required <= manifest["artifacts"].keys())
        manifest["status"] = "PASS" if passed else "FAIL"
        manifest["evidence_sha256"] = {
            str(p.relative_to(record)): digest(p)
            for p in sorted(record.rglob("*")) if p.is_file() and p != manifest_path
        }
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        print(manifest["status"], flush=True)
        return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
