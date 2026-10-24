#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Run WS73 protocol tests in a private, hardware-free VM with provenance."""

import argparse
import datetime
import fcntl
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys


SOURCE = Path(__file__).resolve().parents[3]


def sanitizer_summary(raw, config, kcsan=False):
    """Reject diagnostics even when KTAP cases themselves say PASS."""
    bad = re.search(r"BUG:|WARNING:|UBSAN:|Kernel panic|possible circular|"
                    r"inconsistent lock state|sleeping function called", raw)
    if bad:
        raise ValueError("kernel diagnostic: " + bad.group(0))
    if not kcsan:
        return {}
    required = {"CONFIG_KCSAN=y", "CONFIG_KCSAN_STRICT=y", "CONFIG_KCSAN_WEAK_MEMORY=y",
                "CONFIG_KCSAN_SELFTEST=y", "CONFIG_KCSAN_EARLY_ENABLE=y",
                "CONFIG_KCSAN_SKIP_WATCH=1000", "CONFIG_SMP=y", "CONFIG_PROVE_LOCKING=y",
                "CONFIG_DEBUG_ATOMIC_SLEEP=y", "CONFIG_UBSAN=y", "# CONFIG_KASAN is not set",
                "# CONFIG_SERIO_I8042 is not set", "# CONFIG_MOUSE_PS2 is not set",
                "# CONFIG_KEYBOARD_ATKBD is not set"}
    if not required <= set(config.splitlines()):
        raise ValueError("missing KCSAN instrumentation configuration")
    for marker in ("kcsan: enabled early", "kcsan: strict mode configured",
                   "kcsan: selftest: 3/3 tests passed"):
        if raw.splitlines().count(marker) != 1 or raw.count(marker) != 1:
            raise ValueError("missing/duplicate/malformed KCSAN marker: " + marker)
    return {"early_enabled": True, "selftests_passed": 3, "skip_watch": 1000,
            "scope": "sampled detection only; no race-freedom proof"}


def mutation_summary(raw, usb=True, h4=True, control=False):
    """Require the bounded campaigns' actual counters, not just case names."""
    seeds = {"0073e110", "00387373", "deadbeef", "10203040"}
    specs = {
        "usb": (usb, "mutation seed=", r"mutation seed=([0-9a-f]{8}) inputs=(\d+) "
                r"accepted=(\d+) rejected=(\d+)", ("inputs", "accepted", "rejected")),
        "h4": (h4, "H4 seed=", r"H4 seed=([0-9a-f]{8}) steps=(\d+) complete=(\d+) "
               r"partial=(\d+) errors=(\d+) sticky=(\d+) resets=(\d+)",
               ("steps", "complete", "partial", "errors", "sticky", "resets")),
        "ble_control": (control, "BLE sequence seed=", r"BLE sequence seed=([0-9a-f]{8}) steps=(\d+) "
                        r"success=(\d+) rejected=(\d+) notifications=(\d+) aborts=(\d+) resets=(\d+)",
                        ("steps", "success", "rejected", "notifications", "aborts", "resets")),
    }
    result = {}
    for name, (required, marker, pattern, fields) in specs.items():
        matches = re.findall(pattern + r"(?=\r?$)", raw, flags=re.MULTILINE)
        if raw.count(marker) != len(matches):
            raise ValueError(f"malformed {name} mutation marker")
        if not required:
            if matches:
                raise ValueError(f"unexpected {name} mutation campaign")
            continue
        if len(matches) != 4 or {item[0] for item in matches} != seeds:
            raise ValueError(f"missing/duplicate {name} mutation seed")
        rows = []
        for seed, *values in matches:
            counts = [int(value) for value in values]
            if counts[0] != 4096 or sum(counts[1:]) != 4096 or min(counts[1:]) <= 0:
                raise ValueError(f"incomplete {name} mutation campaign: {seed}")
            rows.append({"seed": seed, **dict(zip(fields, counts))})
        result[name] = rows
    return result


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def archive_previous_test_log(build, record):
    """Keep the old log as history, never as this invocation's execution."""
    previous = build / "test.log"
    if not previous.exists():
        return None
    archived = record / "previous-test.log"
    shutil.copyfile(previous, archived)
    summary = {"path": archived.name, "sha256": digest(archived)}
    previous.unlink()
    return summary


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
    for name in ("python3", "make", "clang", "ld.lld", "llvm-nm", "qemu-system-x86_64"):
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
    parser.add_argument("--kcsan", action="store_true",
                        help="use a separate strict KCSAN build instead of KASAN")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--hci-only", action="store_true",
                      help="run native HCI synthetic data/lifecycle and codec tests")
    mode.add_argument("--combined-hci", action="store_true",
                      help="run protocol and native HCI suites together, without mock clients")
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
        build = work / ("build-kunit-kcsan" if args.kcsan else "build-kunit")
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
        if args.hci_only or args.combined_hci:
            config_text = config_text.replace("CONFIG_WS73_BLE_MOCK_KUNIT_TEST=y",
                                              "# CONFIG_WS73_BLE_MOCK_KUNIT_TEST is not set")
            config_text = config_text.replace("# CONFIG_BT_WS73 is not set", "CONFIG_BT_WS73=y")
            config_text += ("CONFIG_FAULT_INJECTION=y\nCONFIG_FAILSLAB=y\n"
                            "CONFIG_WS73_HCI_KUNIT_TEST=y\n")
        if args.kcsan:
            config_text = "\n".join(line for line in config_text.splitlines()
                                      if not line.startswith("CONFIG_KASAN")) + "\n"
            config_text += ("# CONFIG_KASAN is not set\nCONFIG_KCSAN=y\n"
                            "CONFIG_KCSAN_STRICT=y\nCONFIG_KCSAN_WEAK_MEMORY=y\n"
                            "CONFIG_KCSAN_SELFTEST=y\nCONFIG_KCSAN_EARLY_ENABLE=y\n"
                            "CONFIG_KCSAN_SKIP_WATCH=1000\n"
                            "# CONFIG_SERIO_I8042 is not set\n"
                            "# CONFIG_MOUSE_PS2 is not set\n"
                            "# CONFIG_KEYBOARD_ATKBD is not set\n")
        config.write_text(config_text)
        manifest = {
            "scope": ("native auxiliary/HCI synthetic data/lifecycle and codecs in VM; no real controller/peer"
                      if args.hci_only or args.combined_hci else "synthetic protocol/firmware/endpoint tests in VM; no native HCI registration or peer"),
            "hci_only": args.hci_only,
            "combined_hci": args.combined_hci,
            "core_only": args.core_only,
            "kcsan": args.kcsan,
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
        # kunit.py only replaces test.log after a successful build. Without
        # this separation, a failed build can inherit old PASS counters.
        # The existing global build lock covers both archive and invocation.
        manifest["previous_test_log"] = archive_previous_test_log(build, record)
        manifest_path = record / "manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        print(record, flush=True)
        env = dict(os.environ, CCACHE_DISABLE="1", PYTHONUNBUFFERED="1")
        with (record / "run.log").open("w") as log:
            run = subprocess.run(cmd, cwd=SOURCE, env=env, stdout=log,
                                 stderr=subprocess.STDOUT, check=False)
        manifest["exit_code"] = run.returncode
        manifest["artifact_scope"] = (
            "candidate outputs of successful KUnit invocation; validation pending"
            if run.returncode == 0 else
            "diagnostic build-directory contents; may contain earlier objects/images")
        # Preserve the pre-build patch, then compare the post-build identity.
        after_dir = record / "after"
        after_dir.mkdir()
        manifest["source_after"] = source_identity(after_dir)
        manifest["artifacts"] = {}
        required = {".config", "arch/x86/boot/bzImage", "vmlinux", "test.log",
                    "drivers/misc/ws73/hcc.o", "drivers/misc/ws73/firmware.o",
                    "drivers/misc/ws73/boot.o", "drivers/misc/ws73/boot-loader.o", "drivers/misc/ws73/test.o",
                    "drivers/misc/ws73/usb-wire.o", "drivers/misc/ws73/usb-wire-test.o",
                    "drivers/misc/ws73/usb-tx.o", "drivers/misc/ws73/usb-tx-test.o",
                    "drivers/misc/ws73/runtime.o", "drivers/misc/ws73/runtime-test.o",
                    "drivers/misc/ws73/service.o", "drivers/misc/ws73/service-test.o",
                    "drivers/misc/ws73/ble-control.o", "drivers/misc/ws73/ble-control-test.o",
                    "drivers/misc/ws73/customize.o", "drivers/misc/ws73/customize-test.o",
                    "drivers/misc/ws73/customize-request.o",
                    "drivers/misc/ws73/customize-request-test.o",
                    "drivers/misc/ws73/platform-config.o", "drivers/misc/ws73/platform-config-test.o",
                    "drivers/misc/ws73/startup.o", "drivers/misc/ws73/startup-test.o",
                    "drivers/misc/ws73/board-test.o", "drivers/misc/ws73/boot-loader-test.o", "drivers/misc/ws73/ble.o",
                    "drivers/misc/ws73/ble-test.o", "drivers/misc/ws73/ble-link.o",
                    "drivers/misc/ws73/ble-link-test.o"}
        if not args.core_only:
            required.update({"drivers/bluetooth/btws73_proto.o",
                             "drivers/bluetooth/btws73_test.o",
                             "drivers/misc/ws73/usb.o", "drivers/misc/ws73/usb-io.o"})
        if args.combined_hci:
            required -= {"drivers/misc/ws73/ble-test.o", "drivers/misc/ws73/ble-link-test.o"}
        if args.hci_only:
            required = {name for name in required if not name.endswith(("-test.o", "/test.o"))}
        if args.hci_only or args.combined_hci:
            required.update({"drivers/misc/ws73/ble-hci-test.o", "drivers/misc/ws73/ble-hci-data-test.o",
                             "drivers/bluetooth/btws73.o"})
        if args.kcsan:
            required.update({"kernel/kcsan/core.o", "kernel/kcsan/selftest.o"})
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
                "btws73_h4_state_mutation",
            }
            expected_groups = {
                "ws73-ble-link": {
                    "ws73_link_admission_test", "ws73_link_lifecycle_test",
                    "ws73_link_action_failure_test", "ws73_link_transport_failure_test",
                    "ws73_link_immediate_probe_test", "ws73_link_aborted_probe_test",
                    "ws73_link_deadline_test",
                },
                "ws73-ble-endpoint": {
                    "ws73_ble_lifecycle_test", "ws73_ble_failure_release_test",
                    "ws73_ble_send_failure_test",
                    "ws73_ble_remove_send_test", "ws73_ble_remove_start_test",
                },
                "ws73-boot-loader": {"ws73_loader_preflight_test",
                                     "ws73_loader_identity_mismatch_test",
                                     "ws73_loader_expected_admission_test"},
                "ws73-board-load": {
                    "ws73_board_sequence_test", "ws73_board_faults_test",
                    "ws73_board_admission_test", "ws73_board_echo_test",
                    "ws73_board_echo_reject_test",
                },
                "ws73-platform-config": {"ws73_platform_config_wire_test",
                                         "ws73_platform_config_transport_test"},
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
                    "ws73_boot_ready_echo_test", "ws73_boot_ready_echo_faults_test",
                    "ws73_boot_files_echo_test", "ws73_boot_files_echo_faults_test",
                    "ws73_boot_faults_test", "ws73_boot_admission_test",
                    "ws73_chip_word_test", "ws73_chip_query_test",
                    "ws73_chip_query_faults_test", "ws73_boot_version_test",
                },
                "ws73-usb-wire": {
                    "ws73_usb_control_test", "ws73_usb_single_test",
                    "ws73_usb_single_limits_test", "ws73_usb_aggregate_test",
                    "ws73_usb_aggregate_errors_test",
                    "ws73_usb_mutation_test",
                },
                "ws73-usb-tx": {
                    "ws73_usb_tx_sequence_test", "ws73_usb_tx_faults_test",
                    "ws73_usb_tx_admission_test", "ws73_usb_tx_contention_timeout_test",
                    "ws73_usb_tx_contention_handoff_test",
                },
                "ws73-ble-control": {
                    "ws73_ble_lifecycle_test", "ws73_ble_tx_failure_test",
                    "ws73_ble_reply_failure_test", "ws73_ble_admission_test",
                    "ws73_ble_independent_rx_test", "ws73_ble_state_sequence_test",
                    "ws73_ble_quiesce_lifecycle_test", "ws73_ble_quiesce_failure_test",
                    "ws73_ble_quiesce_admission_test", "ws73_ble_quiesce_independent_rx_test",
                },
                "ws73-service": {
                    "ws73_interrupt_wire_test", "ws73_interrupt_invalid_test",
                    "ws73_notification_valid_test", "ws73_notification_invalid_test",
                    "ws73_notification_values_test", "ws73_notification_aggregate_test",
                },
                "ws73-runtime": {
                    "ws73_version_wire_test", "ws73_version_invalid_test",
                    "ws73_platform_allocation_test",
                    "ws73_runtime_versions_test",
                    "ws73_runtime_query_test", "ws73_runtime_failures_test",
                    "ws73_runtime_reply_errors_test", "ws73_runtime_admission_test",
                    "ws73_runtime_independent_rx_test",
                },
            }
            if args.core_only:
                del expected_groups["btws73-proto"]
            if args.hci_only or args.combined_hci:
                if args.hci_only:
                    expected_groups = {}
                else:
                    del expected_groups["ws73-ble-endpoint"]
                    del expected_groups["ws73-ble-link"]
                expected_groups.update({"btws73-proto": expected,
                                   "ws73-hci-lifecycle": {"ws73_hci_failed_enable_test"},
                                   "ws73-hci-data": {"ws73_hci_two_devices_test", "ws73_hci_consumer_rebind_test", "ws73_hci_data_test", "ws73_hci_queue_limit_test",
                                                     "ws73_hci_byte_limit_test", "ws73_hci_rejected_acl_test",
                                                     "ws73_hci_acl_rx_test", "ws73_hci_active_flush_test",
                                                     "ws73_hci_rx_fault_test", "ws73_hci_tx_nomem_test",
                                                     "ws73_hci_rx_nomem_test", "ws73_hci_link_data_test",
                                                     "ws73_hci_link_short_tx_test",
                                                     "ws73_hci_link_short_action_test", "ws73_hci_link_abort_test"}})
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
        built_config = record / "artifacts/.config"
        built_lines = set(built_config.read_text().splitlines()) if built_config.exists() else set()
        if args.hci_only or args.combined_hci:
            passed = (passed and "CONFIG_BT_WS73=y" in built_lines and
                      not any(line.startswith("CONFIG_WS73_BLE_MOCK_KUNIT_TEST=")
                              for line in built_lines))
        else:
            passed = passed and "CONFIG_WS73_BLE_MOCK_KUNIT_TEST=y" in built_lines
        passed = (passed and run.returncode == 0 and before == manifest["source_after"]
                  and required <= manifest["artifacts"].keys())
        try:
            raw = (record / "artifacts/test.log").read_text()
            manifest["sanitizer"] = sanitizer_summary(
                raw, (record / "artifacts/.config").read_text(), kcsan=args.kcsan)
            if args.kcsan:
                instrumented = ["drivers/misc/ws73/ble.o", "drivers/misc/ws73/ble-link.o"]
                if args.hci_only or args.combined_hci:
                    instrumented.append("drivers/bluetooth/btws73.o")
                manifest["instrumentation"] = {}
                for name in instrumented:
                    symbols = subprocess.check_output(
                        ["llvm-nm", "-u", str(record / "artifacts" / name)], text=True)
                    if not re.search(r"\b__tsan_(?:read|write|volatile|atomic)", symbols):
                        raise ValueError("no memory-access instrumentation: " + name)
                    manifest["instrumentation"][name] = symbols.splitlines()
            manifest["mutation_counts"] = mutation_summary(
                raw,
                usb=not args.hci_only, h4=not args.core_only, control=not args.hci_only)
        except (OSError, ValueError, subprocess.CalledProcessError) as error:
            manifest["validation_error"] = str(error)
            passed = False
        manifest["status"] = "PASS" if passed else "FAIL"
        if passed:
            manifest["artifact_scope"] = "validated outputs of this invocation"
        manifest["evidence_sha256"] = {
            str(p.relative_to(record)): digest(p)
            for p in sorted(record.rglob("*")) if p.is_file() and p != manifest_path
        }
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        print(manifest["status"], flush=True)
        return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
