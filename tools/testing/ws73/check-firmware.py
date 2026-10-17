#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check the selected 1.10.110 original files without touching a USB device."""

import argparse
import hashlib
import json
from pathlib import Path
import sys


# Complete file identities at ws73_SDK d5e68a58fe38, firmware/us/.
# These are hashes, not redistributed firmware or board data.
IMAGES = {
    "ws73.bin": (143956, "7ac0483476bd0c861125929b034a3b9fe5275e6ef8036d37b370eec3f41d1b23"),
    "wifi_cali.bin": (21044, "3d4bfadb1b2a61e1206a68083cbed7b1b109a80cbccab4f4e7825f3a6bce7fc5"),
    "btc_cali.bin": (7772, "4010355847dc14d26c471d7be411c27c226ad3628ca2f1855678e6cfc21c5af4"),
}


def check(directory):
    records = []
    for name, (size, expected) in IMAGES.items():
        path = directory / name
        result = {"file": name, "path": str(path), "status": "FAIL"}
        records.append(result)
        try:
            # Bound reads even if the input is unexpectedly large.
            with path.open("rb") as stream:
                data = stream.read(size + 1)
        except OSError as error:
            result["error"] = str(error)
            continue
        result["size_read"] = len(data)
        result["sha256"] = hashlib.sha256(data).hexdigest()
        if len(data) != size:
            result["error"] = "wrong original-file size (or larger than read limit)"
            continue
        payload_hash = hashlib.sha256(data[64:]).hexdigest()
        result["payload_size"] = len(data) - 64
        result["payload_sha256"] = payload_hash
        if data[:64] != payload_hash.encode("ascii"):
            result["error"] = "invalid 64-byte lowercase SHA-256 header"
        elif result["sha256"] != expected:
            result["error"] = "not the selected 110 original image for this role"
        else:
            result["status"] = "PASS"
    return {
        "selection": "1.10.110 original firmware/us files",
        "scope": "file integrity and pinned identity only; not board compatibility or authenticity",
        "images": records,
        "status": "PASS" if all(r["status"] == "PASS" for r in records) else "FAIL",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="SDK firmware/us directory")
    args = parser.parse_args()
    report = check(args.directory.resolve())
    print(json.dumps(report, indent=2))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
