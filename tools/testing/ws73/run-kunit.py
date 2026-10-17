#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run WS73 protocol tests in a private, hardware-free VM with provenance."""

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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--hci-only", action="store_true",
                      help="run native HCI synthetic data/lifecycle and codec tests")
    mode.add_argument("--core-only", action="store_true",
                        help="verify the shared core with NET and Bluetooth disabled")
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
        config = record / "kunitconfig"
        config_text = (SOURCE / "tools/testing/ws73/kunitconfig").read_text()
        if args.core_only:
            config_text = "\n".join(
                line for line in config_text.splitlines()
                if not line.startswith(("CONFIG_NET=", "CONFIG_BT=", "CONFIG_BT_WS73_",
                                        "CONFIG_USB", "CONFIG_WS73_USB="))
            ) + "\n# CONFIG_NET is not set\n# CONFIG_BT is not set\n"
        if args.hci_only:
            config_text = config_text.replace("CONFIG_WS73_KUNIT_TEST=y", "# CONFIG_WS73_KUNIT_TEST is not set")
            config_text = config_text.replace("# CONFIG_BT_WS73 is not set", "CONFIG_BT_WS73=y")
            config_text += ("CONFIG_FAULT_INJECTION=y\nCONFIG_FAILSLAB=y\n"
                            "CONFIG_WS73_HCI_KUNIT_TEST=y\n")
        config.write_text(config_text)
        manifest = {
            "scope": ("native auxiliary/HCI synthetic data/lifecycle and codecs in VM; no real controller/peer"
                      if args.hci_only else "synthetic protocol/firmware/endpoint tests in VM; no native HCI registration or peer"),
            "hci_only": args.hci_only,
            "core_only": args.core_only,
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
               f"--kunitconfig={config}",
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
        required = {".config", "arch/x86/boot/bzImage", "vmlinux", "test.log",
                    "drivers/misc/ws73/hcc.o", "drivers/misc/ws73/firmware.o",
                    "drivers/misc/ws73/boot.o", "drivers/misc/ws73/test.o",
                    "drivers/misc/ws73/usb-wire.o", "drivers/misc/ws73/usb-wire-test.o",
                    "drivers/misc/ws73/usb-tx.o", "drivers/misc/ws73/usb-tx-test.o",
                    "drivers/misc/ws73/runtime.o", "drivers/misc/ws73/runtime-test.o",
                    "drivers/misc/ws73/service.o", "drivers/misc/ws73/service-test.o",
                    "drivers/misc/ws73/ble-control.o", "drivers/misc/ws73/ble-control-test.o",
                    "drivers/misc/ws73/customize.o", "drivers/misc/ws73/customize-test.o",
                    "drivers/misc/ws73/customize-request.o",
                    "drivers/misc/ws73/customize-request-test.o",
                    "drivers/misc/ws73/startup.o", "drivers/misc/ws73/startup-test.o",
                    "drivers/misc/ws73/board-test.o", "drivers/misc/ws73/ble.o",
                    "drivers/misc/ws73/ble-test.o", "drivers/misc/ws73/ble-link.o",
                    "drivers/misc/ws73/ble-link-test.o"}
        if not args.core_only:
            required.update({"drivers/bluetooth/btws73_proto.o",
                             "drivers/bluetooth/btws73_test.o",
                             "drivers/misc/ws73/usb.o"})
        if args.hci_only:
            required = {name for name in required if not name.endswith(("-test.o", "/test.o"))}
            required.update({"drivers/misc/ws73/ble-hci-test.o", "drivers/misc/ws73/ble-hci-data-test.o",
                             "drivers/bluetooth/btws73.o"})
        for name in sorted(required):
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
                "btws73_h4_splits", "btws73_h4_coalesced", "btws73_h4_errors",
            }
            expected_groups = {
                "ws73-ble-link": {
                    "ws73_link_admission_test", "ws73_link_lifecycle_test",
                    "ws73_link_action_failure_test", "ws73_link_transport_failure_test",
                },
                "ws73-ble-endpoint": {
                    "ws73_ble_lifecycle_test", "ws73_ble_failure_release_test",
                    "ws73_ble_send_failure_test",
                },
                "ws73-board-load": {
                    "ws73_board_sequence_test", "ws73_board_faults_test",
                    "ws73_board_admission_test",
                },
                "ws73-startup": {
                    "ws73_startup_sequence_test", "ws73_startup_missing_test",
                    "ws73_startup_failures_test", "ws73_startup_fault_priority_test",
                    "ws73_startup_admission_test", "ws73_startup_independent_rx_test",
                },
                "ws73-custom-request": {
                    "ws73_custom_request_lifecycle_test", "ws73_custom_request_tx_faults_test",
                    "ws73_custom_request_reply_faults_test", "ws73_custom_request_admission_test",
                    "ws73_custom_request_independent_rx_test",
                },
                "ws73-customize": {
                    "ws73_customize_wire_test", "ws73_customize_encode_errors_test",
                    "ws73_customize_validate_errors_test", "ws73_customize_transport_test",
                },
                "btws73-proto": expected,
                "ws73-fw-boot": {
                    "ws73_profiles_test", "ws73_file_test", "ws73_boot_sequence_test",
                    "ws73_boot_faults_test", "ws73_boot_admission_test",
                    "ws73_chip_word_test", "ws73_chip_query_test",
                    "ws73_chip_query_faults_test",
                },
                "ws73-usb-wire": {
                    "ws73_usb_control_test", "ws73_usb_single_test",
                    "ws73_usb_single_limits_test", "ws73_usb_aggregate_test",
                    "ws73_usb_aggregate_errors_test",
                },
                "ws73-usb-tx": {
                    "ws73_usb_tx_sequence_test", "ws73_usb_tx_faults_test",
                    "ws73_usb_tx_admission_test",
                },
                "ws73-ble-control": {
                    "ws73_ble_lifecycle_test", "ws73_ble_tx_failure_test",
                    "ws73_ble_reply_failure_test", "ws73_ble_admission_test",
                    "ws73_ble_independent_rx_test",
                },
                "ws73-service": {
                    "ws73_interrupt_wire_test", "ws73_interrupt_invalid_test",
                    "ws73_notification_valid_test", "ws73_notification_invalid_test",
                    "ws73_notification_values_test", "ws73_notification_aggregate_test",
                },
                "ws73-runtime": {
                    "ws73_version_wire_test", "ws73_version_invalid_test",
                    "ws73_runtime_versions_test",
                    "ws73_runtime_query_test", "ws73_runtime_failures_test",
                    "ws73_runtime_reply_errors_test", "ws73_runtime_admission_test",
                    "ws73_runtime_independent_rx_test",
                },
            }
            if args.core_only:
                del expected_groups["btws73-proto"]
            if args.hci_only:
                expected_groups = {"btws73-proto": expected,
                                   "ws73-hci-lifecycle": {"ws73_hci_failed_enable_test"},
                                   "ws73-hci-data": {"ws73_hci_data_test", "ws73_hci_queue_limit_test",
                                                     "ws73_hci_byte_limit_test", "ws73_hci_rejected_acl_test",
                                                     "ws73_hci_acl_rx_test", "ws73_hci_active_flush_test",
                                                     "ws73_hci_rx_fault_test", "ws73_hci_tx_nomem_test",
                                                     "ws73_hci_rx_nomem_test"}}
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
        if args.core_only:
            built_config = record / "artifacts/.config"
            lines = built_config.read_text().splitlines() if built_config.exists() else []
            passed = (passed and "# CONFIG_NET is not set" in lines
                      and not any(line.startswith(("CONFIG_BT=", "CONFIG_BT_WS73_"))
                                  for line in lines))
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
