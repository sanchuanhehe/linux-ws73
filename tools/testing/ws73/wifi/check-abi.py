#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compile pinned SDK declarations to objects; never run code or access USB.

Private records contain SDK excerpts and must not be uploaded with the driver.
This checks candidate compiler layouts, not the original firmware producer ABI.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


SOURCE = Path(__file__).resolve().parents[4]
SDK = SOURCE.parent / "ws73_SDK"
PIN = "d5e68a58fe38c2b04a16769c69de1e8a80e86d74"
FILES = {
    "osal_types.h": "driver/platform/osal/include/osal_types.h",
    "frw_msg_rom.h": "driver/wifi/device/source/inc/romable/frw_msg_rom.h",
    "frw_hmac.h": "driver/wifi/inc/frw/frw_hmac.h",
    "hal_rx.h": "driver/wifi/device/source/inc/romable/device_common/hal_common_ops_device_rom.h",
    "dmac_rx.h": "driver/wifi/device/source/inc/romable/device_common/dmac_ext_if_device_rom.h",
    "tx_status.h": "driver/wifi/device/source/inc/romable/dmac_ext_if_type_rom.h",
}
TARGETS = {
    "x86_64-linux-gnu": (8, False),
    "i386-linux-gnu": (4, False),
    "aarch64-linux-gnu": (8, False),
    "riscv32-unknown-elf": (4, False),
    "powerpc64-linux-gnu": (8, True),
}
ELF_MACHINES = {"x86_64-linux-gnu": 62, "i386-linux-gnu": 3,
                "aarch64-linux-gnu": 183, "riscv32-unknown-elf": 243,
                "powerpc64-linux-gnu": 21}
PROBE = r'''
#include "frw_msg_rom.h"
#include "comm-excerpt.h"
#include "descriptor-excerpt.h"
#define SECTION(name) __attribute__((section(name), used))
/* Deliberate field sentinels, not valid commands for transmission. */
const frw_hcc_extend_hdr_stru ext SECTION(".ws73_ext") = {
    .msg.msg_id = 0x1234, .vap_id = 10, .msg_type = 3,
    .data_len = 0x5678, .pay_type = 1, .sub_type = 2, .rsv = 0x9abc,
};
const frw_msg_adapt_comm comm SECTION(".ws73_comm") = {
    .msg_id = 0x2468, .vap_id = 7, .sync = 1, .type = 1,
    .cb_ret = -123, .msg_seq = 0x01234567,
    .cfg = { .data = 0, .rsp = 0, .data_len = 16, .rsp_buf_len = 32,
             .rsp_len = 0, .sync = 1, .type = 0x12, .rsv = 0x5a },
};
const unsigned char sizes[] SECTION(".ws73_sizes") = {
    sizeof(void *), sizeof(ext), sizeof(frw_msg_host), sizeof(comm),
    sizeof(frw_msg), sizeof(frw_msg_adapt),
    __builtin_offsetof(frw_msg_adapt, cfg),
};
_Static_assert(sizeof(ext) == 8, "FRW extension");
_Static_assert(sizeof(comm) == 28, "fixed-width communication envelope");
_Static_assert(__builtin_offsetof(frw_msg_adapt_comm, cfg) == 12, "cfg offset");
const dmac_rx_ctl_stru rx SECTION(".ws73_rx") = {
    .rx_info = { .vap_id = 17, .amsdu_enable = 1, .is_first_buffer = 1,
        .msdu_in_buffer = 2, .ta_user_idx = 19, .tid = 2, .is_key_frame = 1,
        .mac_header_len = 26, .is_last_buffer = 1, .frame_len = 0x1234,
        .mac_vap_id = 5, .buff_nums = 3, .channel_number = 11,
        .ftm_frame = 1, .stream_id = 5, .xwGROzPmwOmPIOzoR3_ = 0xabc,
        .latency_timestamp_us = 0x123456, .latency_index = 0x78,
        .da_user_idx = 0x3456, .pul_mac_hdr_start_addr = 0x89abcdef },
    .rx_status = { .cipher_protocol_type = 3, .dscr_status = 10,
        .channel_code = 1, .stbc = 2, .gi = 1, .ampdu = 1, .sounding_mode = 2,
        .ext_spatial_streams = 2, .smoothing = 1, .freq_bandwidth_mode = 9,
        .preabmle = 1, .rsp_flag = 1, .he_flag = 1, .last_mpdu_flag = 1,
        .he_ltf_type = 2, .dcm = 1 },
    .rx_statistic = { .rssi_dbm = -42, .nss_rate.legacy_rate = {
        .legacy_rate = 11, .protocol_mode = 2 }, .snr_ant0 = 7, .snr_ant1 = -8,
        .ant0_rssi = -43, .ant1_rssi = -44, .phase_incr = -1234 },
};
const dmac_tx_status_stru tx_status SECTION(".ws73_tx_status") = {
    .dscr_status = 0x10203040, .frame_type = 5, .mgmt_frame_id = 9,
    .user_idx = 0x1234,
};
_Static_assert(sizeof(hal_rx_ctl_stru) == 20, "RX info size");
_Static_assert(sizeof(hal_rx_status_stru) == 4, "RX status size");
_Static_assert(sizeof(hal_rx_statistic_stru) == 8, "RX statistics size");
_Static_assert(sizeof(dmac_rx_ctl_stru) == 32, "RX control size");
_Static_assert(__builtin_offsetof(dmac_rx_ctl_stru, rx_status) == 20, "RX status offset");
_Static_assert(__builtin_offsetof(dmac_rx_ctl_stru, rx_statistic) == 24, "RX statistics offset");
_Static_assert(__builtin_offsetof(hal_rx_ctl_stru, frame_len) == 4, "RX frame length offset");
_Static_assert(__builtin_offsetof(hal_rx_ctl_stru, pul_mac_hdr_start_addr) == 16, "device address offset");
_Static_assert(sizeof(dmac_tx_status_stru) == 8, "TX status size");
_Static_assert(__builtin_offsetof(dmac_tx_status_stru, user_idx) == 6, "TX user offset");
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def declaration(text, name, start="typedef struct {"):
    """Extract one pinned scalar declaration, preserving nested struct bodies."""
    end = "} " + name + ";" if start == "typedef struct {" else "};\ntypedef struct mac_rx_ctl  hal_rx_ctl_stru;"
    assert text.count(end) == 1, "ambiguous declaration: " + name
    last = text.index(end) + len(end)
    first = text.rfind(start, 0, last)
    assert first >= 0
    body = text[first:last]
    assert body.count("{") == body.count("}"), "unbalanced declaration: " + name
    return body


def main():
    if not __debug__:
        raise RuntimeError("Run without Python optimization")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    record = SOURCE.parent / ".dev/ws73-upstream/records" / ("wifi-abi-" + stamp)
    record.mkdir(parents=True)
    print(record, flush=True)
    manifest = {"status": "INCOMPLETE", "sdk_commit": PIN,
                "scope": "Clang object layout only; no firmware, device or runtime qualification",
                "sources": {}, "tools": {}, "targets": {}, "artifacts": {}}
    try:
        manifest["driver_head"] = subprocess.check_output(
            ["git", "-C", str(SOURCE), "rev-parse", "HEAD"], text=True).strip()
        manifest["driver_status"] = subprocess.check_output(
            ["git", "-C", str(SOURCE), "status", "--porcelain"], text=True)
        assert not manifest["driver_status"], "commit sources before qualification"
        for tool in ("clang", "llvm-objcopy"):
            path = Path(shutil.which(tool)).resolve()
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
        for name, relative in FILES.items():
            original = SDK / relative
            pinned = subprocess.check_output(["git", "-C", str(SDK), "show", f"{PIN}:{relative}"])
            assert original.read_bytes() == pinned, "SDK differs from pin: " + relative
            (record / name).write_bytes(pinned)
            manifest["sources"][relative] = sha(original)
        text = (record / "frw_hmac.h").read_text()
        declarations = []
        for name in ("frw_msg_host", "frw_msg_adapt_comm"):
            matches = re.findall(r"typedef struct \{[^{}]*\} " + name + r";", text)
            assert len(matches) == 1, "ambiguous declaration: " + name
            declarations.append(matches[0])
        (record / "comm-excerpt.h").write_text("\n".join(declarations) + "\n")
        # These three HAL declarations each have an explicit pack(1) scope
        # in the pinned header. The two DMAC outer declarations are unpacked.
        hal = (record / "hal_rx.h").read_text()
        rx = [declaration(hal, "hal_rx_ctl_stru", "struct mac_rx_ctl {"),
              declaration(hal, "hal_rx_status_stru"), declaration(hal, "hal_rx_statistic_stru")]
        outer = [declaration((record / "dmac_rx.h").read_text(), "dmac_rx_ctl_stru"),
                 declaration((record / "tx_status.h").read_text(), "dmac_tx_status_stru")]
        (record / "descriptor-excerpt.h").write_text("#pragma pack(push, 1)\n" +
            "\n".join(rx) + "\n#pragma pack(pop)\n" + "\n".join(outer) + "\n")
        (record / "probe.c").write_text(PROBE)
        shutil.copyfile(__file__, record / "check-abi.py")
        for target, (pointer_size, big_endian) in TARGETS.items():
            obj = record / (target + ".o")
            compile_cmd = ["clang", "-target", target, "-ffreestanding", "-std=c11",
                           "-Wall", "-Wextra", "-Werror", "-c", str(record / "probe.c"), "-o", str(obj)]
            subprocess.run(compile_cmd, check=True, capture_output=True, text=True,
                           env=dict(os.environ, CCACHE_DISABLE="1"))
            header = obj.read_bytes()[:20]
            assert len(header) == 20 and header[:4] == b"\x7fELF", "missing ELF object"
            byte_order = "big" if big_endian else "little"
            elf = {"class": header[4], "encoding": header[5],
                   "type": int.from_bytes(header[16:18], byte_order),
                   "machine": int.from_bytes(header[18:20], byte_order)}
            assert elf == {"class": 2 if pointer_size == 8 else 1,
                           "encoding": 2 if big_endian else 1,
                           "type": 1, "machine": ELF_MACHINES[target]}, (target, elf)
            extract_cmd = ["llvm-objcopy"]
            for field in ("ext", "comm", "sizes", "rx", "tx_status"):
                extract_cmd += [f"--dump-section=.ws73_{field}={record / (target + '.' + field)}"]
            extract_cmd += [str(obj), str(record / (target + "-copy.o"))]
            subprocess.run(extract_cmd, check=True, capture_output=True, text=True)
            expected = {
                "ext": "1234a35678129abc" if big_endian else "34123a785621bc9a",
                "comm": ("24680790ffffff85012345670000000000000000001000200000925a" if big_endian
                         else "6824070385ffffff674523010000000000000000100020000000255a"),
                "sizes": bytes([pointer_size, 8, 16, 28, 24 if pointer_size == 8 else 16,
                                40 if pointer_size == 8 else 28, 16 if pointer_size == 8 else 12]).hex(),
                "rx": ("8e029d691234530bdabc12345678345689abcdef3aceb3bad6b207f8d5d4fb2e"
                       if big_endian else
                       "7102d39a3412350bcbab563412785634efcdab89a3adce6dd68b07f8d5d42efb"),
                "tx_status": "1020304005091234" if big_endian else "4030201005093412",
            }
            observed = {field: (record / (target + "." + field)).read_bytes().hex() for field in expected}
            manifest["targets"][target] = {"compile": compile_cmd, "extract": extract_cmd,
                                            "elf": elf, "expected": expected, "observed": observed}
            assert observed == expected, (target, observed, expected)
        for relative, digest in manifest["sources"].items():
            assert sha(SDK / relative) == digest, "SDK changed during audit"
        manifest["status"] = "COMPILER_LAYOUT_CHECK_PASS_NOT_FIRMWARE_ABI_PROOF"
    except BaseException as error:
        manifest["status"] = "FAIL"
        manifest["error"] = repr(error)
        if isinstance(error, subprocess.CalledProcessError):
            manifest["compiler_stdout"] = error.stdout
            manifest["compiler_stderr"] = error.stderr
        raise
    finally:
        for path in record.iterdir():
            if path.is_file() and path.name != "manifest.json":
                manifest["artifacts"][path.name] = sha(path)
        (record / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(manifest["status"], flush=True)


if __name__ == "__main__":
    main()
