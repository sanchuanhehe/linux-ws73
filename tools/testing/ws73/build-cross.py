#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compile/link actual WS73 modules across architectures; never run or install."""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

SOURCE = Path(__file__).resolve().parents[3]
WORK = SOURCE.parent / ".dev/ws73-upstream"
PROFILES = {
    "arm64-clang": ("arm64", True, 2, 1, 183, ["CONFIG_64BIT=y"]),
    "i386-clang": ("i386", True, 1, 1, 3, ["CONFIG_X86_32=y"]),
    "ppc64be-clang": ("powerpc", True, 2, 2, 21,
                      ["CONFIG_PPC64=y", "CONFIG_CPU_BIG_ENDIAN=y"]),
    "x86_64-gcc": ("x86_64", False, 2, 1, 62, ["CONFIG_64BIT=y"]),
}
COMMON = ["CONFIG_MODULES=y", "CONFIG_NET=y", "CONFIG_BT=m", "CONFIG_BT_LE=y",
          "# CONFIG_BT_BREDR is not set",
          "CONFIG_USB_SUPPORT=y", "CONFIG_USB=m", "CONFIG_WS73_USB=m",
          "CONFIG_BT_WS73=m"]
RESOLVED = ["CONFIG_WS73_CORE=m", "CONFIG_WS73_USB_IO=m", "CONFIG_BT_WS73_PROTO=m"]
MODULES = ["drivers/misc/ws73/ws73-core.ko", "drivers/misc/ws73/ws73-usb.ko",
           "drivers/misc/ws73/ws73-usb-io.ko", "drivers/bluetooth/btws73-hci.ko",
           "drivers/bluetooth/btws73_proto.ko", "net/bluetooth/bluetooth.ko"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(["git", "-C", str(SOURCE), *args], text=True).strip()


def elf(path, klass, endian, machine, relocatable):
    header = path.read_bytes()[:20]
    assert header[:6] == b"\x7fELF" + bytes([klass, endian]), path
    order = "little" if endian == 1 else "big"
    assert int.from_bytes(header[18:20], order) == machine, path
    kind = int.from_bytes(header[16:18], order)
    assert kind == 1 if relocatable else kind in (2, 3), path
    return {"class": klass, "endian": endian, "machine": machine, "type": kind}


def build(profile):
    arch, llvm, klass, endian, machine, extra = PROFILES[profile]
    assert not git("status", "--porcelain"), "commit sources before qualification"
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    record = WORK / "records" / ("cross-" + profile + "-" + stamp)
    record.mkdir(parents=True)
    output = WORK / ("build-cross-" + profile)
    output.mkdir(exist_ok=True)
    fragment = record / "requested.config"
    fragment.write_text("\n".join(COMMON + extra) + "\n")
    shutil.copyfile(__file__, record / "build-cross.py")
    manifest = {"status": "INCOMPLETE", "source": git("rev-parse", "HEAD"),
                "profile": profile, "jobs": 4, "commands": [], "tools": {},
                "artifacts": {}, "elf": {}, "scope": "compile-link-only"}

    def save():
        (record / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    print(record, flush=True)
    save()
    try:
        for tool in ("python3", "make", "clang", "ld.lld", "llvm-ar", "llvm-nm",
                     "llvm-objcopy", "llvm-objdump", "gcc", "ld", "ar", "nm", "objcopy"):
            path = Path(shutil.which(tool))
            manifest["tools"][tool] = {
                "path": str(path), "sha256": sha(path),
                "version": subprocess.check_output([tool, "--version"], text=True),
                "candidates": {str(p): sha(p) for p in sorted({
                    (Path(entry) / tool).resolve() for entry in os.get_exec_path()
                    if (Path(entry) / tool).is_file()})}}
        env = dict(os.environ, CCACHE_DISABLE="1", KCONFIG_ALLCONFIG=str(fragment),
                   KCONFIG_DEFCONFIG_LIST="/dev/null", KCONFIG_CONFIG=".config")
        base = ["make", "-C", str(SOURCE), f"O={output}", f"ARCH={arch}", "W=1", "-j4"]
        base += ["LLVM=1"] if llvm else ["CC=gcc", "HOSTCC=gcc", "LD=ld"]
        with (record / "build.log").open("w") as log:
            for target in ("allnoconfig", "vmlinux", "modules"):
                command = base + [target]
                manifest["commands"].append(command)
                save()
                subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                               check=True, timeout=1800)
                if target == "allnoconfig":
                    actual = (output / ".config").read_text().splitlines()
                    for line in COMMON + RESOLVED + extra:
                        assert line in actual, "unresolved config: " + line
                    for line in actual:
                        assert not (line.startswith("CONFIG_") and
                                    ("KUNIT" in line or "WS73" in line and "TEST" in line)), line
                    assert "CONFIG_BROKEN=y" not in actual
        assert not git("status", "--porcelain")
        assert git("rev-parse", "HEAD") == manifest["source"]
        for name in ["vmlinux"] + MODULES:
            manifest["elf"][name] = elf(output / name, klass, endian, machine, name.endswith(".ko"))
        manifest["status"] = "PASS"
    except BaseException as error:
        manifest["status"] = "FAIL"
        manifest["error"] = repr(error)
        raise
    finally:
        # Preserve partial evidence on failure too; only PASS validates freshness.
        for name in [".config", "vmlinux", "Module.symvers", "modules.order"] + MODULES:
            if (output / name).is_file():
                dest = record / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(output / name, dest)
                manifest["artifacts"][name] = sha(dest)
        for name in ("build-cross.py", "requested.config", "build.log"):
            if (record / name).is_file():
                manifest["artifacts"][name] = sha(record / name)
        save()
    print(profile + " PASS", flush=True)


def main():
    if not __debug__:
        raise RuntimeError("Run without Python optimization")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", choices=[*PROFILES, "all"])
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        for profile in PROFILES if args.profile == "all" else [args.profile]:
            build(profile)


if __name__ == "__main__":
    main()
