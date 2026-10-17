#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build a tagged-VM WS73 RAM trial kernel and external test module; no device I/O."""
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

SOURCE = Path(__file__).resolve().parents[4]
WORK = SOURCE.parent / ".dev/ws73-upstream"
BUILD = WORK / "build-hardware"
CONFIG = Path(__file__).with_name("hardware.config")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(["git", "-C", str(SOURCE), *args], text=True).strip()


def main():
    if not __debug__:
        raise RuntimeError("Run without Python optimization")
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        assert not git("status", "--porcelain"), "commit sources before qualifying"
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        record = WORK / "records" / ("hardware-build-" + stamp)
        record.mkdir(parents=True)
        print(record, flush=True)
        BUILD.mkdir(exist_ok=True)
        shutil.copyfile(CONFIG, record / "hardware.config")
        shutil.copyfile(__file__, record / "build.py")
        manifest = {"status": "INCOMPLETE", "source": git("rev-parse", "HEAD"),
                    "jobs": 4, "command": [], "artifacts": {}, "tools": {},
                    "config_sha256": sha(CONFIG), "builder_sha256": sha(Path(__file__))}

        def save():
            (record / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

        save()
        try:
            for tool in ("python3", "make", "clang", "ld.lld"):
                path = Path(shutil.which(tool))
                manifest["tools"][tool] = {
                    "path": str(path), "sha256": sha(path),
                    "version": subprocess.check_output([tool, "--version"], text=True),
                    "path_candidates_sha256": {
                        str(candidate): sha(candidate) for candidate in sorted({
                            (Path(entry) / tool).resolve() for entry in os.get_exec_path()
                            if (Path(entry) / tool).is_file()
                        })
                    },
                }
            env = dict(os.environ, CCACHE_DISABLE="1", KCONFIG_ALLCONFIG=str(CONFIG),
                       KCONFIG_DEFCONFIG_LIST="/dev/null", KCONFIG_CONFIG=".config")
            with (record / "build.log").open("w") as log:
                for target in ("allnoconfig", "bzImage", "modules"):
                    command = ["make", "-C", str(SOURCE), f"O={BUILD}", "ARCH=x86_64",
                               "LLVM=1", "W=1", "-j4", target]
                    manifest["command"].append(command)
                    save()
                    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                   env=env, check=True)
                    # Check dependencies before accepting or building a weakened configuration.
                    if target == "allnoconfig":
                        actual = (BUILD / ".config").read_text().splitlines()
                        for line in CONFIG.read_text().splitlines():
                            if line.startswith("CONFIG_") or line.startswith("# CONFIG_"):
                                assert line in actual, "unresolved config: " + line
            module = WORK / "build-hardware-module"
            module.mkdir(exist_ok=True)
            for name in ("loader.c", "guard.h", "guard-test.c", "ble-trial.c", "ble-profile.h", "ble-profile-test.c", "Kbuild"):
                original = Path(__file__).with_name(name)
                shutil.copyfile(original, module / name)
                shutil.copyfile(original, record / name)
            command = ["make", "-C", str(SOURCE), f"O={BUILD}", f"M={module}",
                       f"WS73_SOURCE={SOURCE}", "ARCH=x86_64", "LLVM=1", "W=1", "-j4", "modules"]
            manifest["command"].append(command)
            with (record / "module-build.log").open("w") as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, check=True)
            for name in ("ws73-ram-trial.ko", "loader.o", "ws73-ram-trial-guard-test.ko", "guard-test.o",
                         "ws73-ble-trial.ko", "ble-trial.o", "ws73-ble-profile-test.ko",
                         "ble-profile-test.o"):
                shutil.copyfile(module / name, record / name)
                manifest["artifacts"][name] = sha(record / name)
            assert not git("status", "--porcelain")
            assert git("rev-parse", "HEAD") == manifest["source"]
            names = [".config", "arch/x86/boot/bzImage", "vmlinux", "Module.symvers"]
            names += [f"drivers/misc/ws73/{name}.o" for name in
                      ("usb", "usb-io", "boot", "boot-loader", "firmware", "runtime", "usb-tx", "usb-wire",
                       "platform-config", "service", "ble-control", "customize", "customize-request", "startup", "ble", "ble-link")]
            for name in names:
                dest = record / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(BUILD / name, dest)
                manifest["artifacts"][name] = sha(dest)
            manifest["status"] = "PASS"
        except BaseException as error:
            manifest["status"] = "FAIL"
            manifest["error"] = repr(error)
            raise
        finally:
            save()
        (WORK / "hardware-build-record").write_text(str(record) + "\n")
        print("PASS", flush=True)


if __name__ == "__main__":
    main()
