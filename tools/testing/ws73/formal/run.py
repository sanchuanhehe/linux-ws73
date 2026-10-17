#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Bounded CBMC checks of production HCC scalar converters; no kernel execution."""
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import signal
import subprocess

SOURCE = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
WORK = SOURCE.parent / ".dev/ws73-upstream"
EXPECTED = {
    "verify_chip_reply": {"chip acceptance matches wire contract", "chip result is success or error",
                          "chip failure leaves output unchanged", "chip success preserves package and bus",
                          "chip preserves prefix guard", "chip leaves input unchanged"},
    "verify_version_reply": {"version acceptance matches wire contract", "version result is success or error",
                             "version failure leaves output unchanged", "version success preserves all bits",
                             "version preserves prefix guard", "version leaves input unchanged"},
    "verify_chip_query": {"query return follows pointer and capacity contract", "query preserves prefix guard",
                          "query failure leaves output unchanged", "query bytes match wire contract"},
    "verify_version_query": {"query return follows pointer and capacity contract", "query preserves prefix guard",
                             "query failure leaves output unchanged", "query bytes match wire contract"},
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(["git", "-C", str(SOURCE), *args], text=True).strip()


def validate(raw, returncode, function, negative=None):
    messages = json.loads(raw)
    if not isinstance(messages, list):
        raise ValueError("invalid CBMC document")
    if any(m.get("messageType") in ("ERROR", "WARNING") or
           "unsupported" in m.get("messageText", "").lower() for m in messages):
        raise ValueError("CBMC warning/error/unsupported operation")
    conclusions = [m["cProverStatus"] for m in messages if "cProverStatus" in m]
    groups = [m["result"] for m in messages if "result" in m]
    if len(groups) != 1 or not groups[0]:
        raise ValueError("missing/duplicate/empty property result")
    props = groups[0]
    ids = [p["property"] for p in props]
    if len(set(ids)) != len(ids):
        raise ValueError("duplicate property")
    contracts = {p["description"] for p in props if p["description"].startswith("WS73 ")}
    if contracts != {"WS73 " + name for name in EXPECTED[function]}:
        raise ValueError("missing/extra harness contract")
    if any(p["status"] not in ("SUCCESS", "FAILURE") for p in props):
        raise ValueError("unknown/incomplete property status")
    if negative is None:
        if returncode != 0 or conclusions != ["success"] or any(p["status"] != "SUCCESS" for p in props):
            raise ValueError("unproved property or unsuccessful process")
    elif (returncode != 10 or conclusions != ["failure"] or not any(
            p["status"] == "FAILURE" and p["description"] == "WS73 " + negative for p in props)):
        raise ValueError("negative control did not expose its intended contract violation")
    if negative and any(p["status"] == "FAILURE" and p["description"] != "WS73 " + negative for p in props):
        raise ValueError("negative control also failed an unintended property")
    return {"properties": len(props), "contracts": sorted(contracts),
            "failed": [p["property"] for p in props if p["status"] == "FAILURE"]}


def limits():
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3, 4 * 1024**3))


def main():
    if not __debug__:
        raise RuntimeError("run without Python optimization")
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        assert not git("status", "--porcelain"), "commit before qualification"
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        record = WORK / "records" / ("cbmc-hcc-" + stamp)
        record.mkdir(parents=True)
        print(record, flush=True)
        files = [SOURCE / "drivers/misc/ws73/hcc.c", SOURCE / "include/linux/ws73.h",
                 HERE / "hcc.c", HERE / "run.py", *sorted((HERE / "shim").rglob("*.h"))]
        manifest = {"status": "INCOMPLETE", "source": git("rev-parse", "HEAD"), "sources": {},
                    "tools": {}, "attempts": [], "timeout_seconds": 60, "address_space_limit_gib": 4,
                    "scope": "four scalar HCC converters; valid retained caller objects; modeled kernel helpers; no concurrency or hardware"}
        def save():
            (record / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        save()
        try:
            for path in files:
                name = str(path.relative_to(SOURCE))
                dest = record / "inputs" / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest)
                manifest["sources"][name] = sha(path)
            for tool in ("cbmc", "gcc", "python3"):
                path = Path(shutil.which(tool))
                manifest["tools"][tool] = {"path": str(path), "sha256": sha(path),
                    "version": subprocess.check_output([tool, "--version"], text=True),
                    "candidates": {str(p): sha(p) for p in sorted({
                        (Path(d) / tool).resolve() for d in os.get_exec_path() if (Path(d) / tool).is_file()})}}

            def run(name, harness, function, arch, negative=None):
                command = ["cbmc", str(harness), "-I" + str(HERE / "shim"), "-I" + str(SOURCE / "include"),
                           "--function", function, "--arch", arch, "--bounds-check", "--pointer-check",
                           "--pointer-overflow-check", "--signed-overflow-check", "--unsigned-overflow-check",
                           "--conversion-check", "--div-by-zero-check", "--memory-leak-check",
                           "--unwinding-assertions", "--unwind", "33", "--json-ui", "--trace", "--drop-unused-functions"]
                item = {"name": name, "function": function, "arch": arch, "command": command,
                        "negative_contract": negative, "status": "INCOMPLETE"}
                manifest["attempts"].append(item)
                save()
                out, err = record / (name + ".json"), record / (name + ".stderr")
                with out.open("w") as stdout, err.open("w") as stderr:
                    proc = subprocess.Popen(command, stdout=stdout, stderr=stderr,
                        env=dict(os.environ, CCACHE_DISABLE="1"), start_new_session=True, preexec_fn=limits)
                    try:
                        item["exit_code"] = proc.wait(timeout=60)
                    except subprocess.TimeoutExpired:
                        os.killpg(proc.pid, signal.SIGKILL)
                        proc.wait()
                        item["status"] = "NOT_PROVEN_TIMEOUT"
                        raise RuntimeError("CBMC timeout: " + name)
                item["stdout_sha256"], item["stderr_sha256"] = sha(out), sha(err)
                if err.read_text().strip():
                    raise ValueError("unexpected CBMC stderr: " + name)
                item.update(validate(out.read_text(), item["exit_code"], function, negative))
                item["status"] = "EXPECTED_COUNTEREXAMPLE" if negative else "BOUNDED_PASS"
                save()

            for arch in ("i386", "x86_64"):
                for function in EXPECTED:
                    run(arch + "-" + function, HERE / "hcc.c", function, arch)
            production = (SOURCE / "drivers/misc/ws73/hcc.c").read_text()
            mutations = [
                ("chip-query-tag", "verify_chip_query", "put_unaligned_le16(16, out + 4)",
                 "put_unaligned_le16(15, out + 4)", "query bytes match wire contract"),
                ("version-query-role", "verify_version_query", "out[8] = 1;", "out[8] = 0;",
                 "query bytes match wire contract"),
                ("chip-partial-publish", "verify_chip_reply", "if (len < 18 || len > BTWS73_RX_MAX_RECORD_SIZE)",
                 "identity->package = 0;\n\tif (len < 18 || len > BTWS73_RX_MAX_RECORD_SIZE)",
                 "chip failure leaves output unchanged"),
                ("chip-record-bound", "verify_chip_reply", "record_len < 18 || record_len > len", "record_len < 18",
                 "chip acceptance matches wire contract"),
                ("version-value", "verify_version_reply", "parsed.rom = get_unaligned_le32(data + 20)",
                 "parsed.rom = get_unaligned_le32(data + 16)", "version success preserves all bits"),
            ]
            for name, function, old, new, contract in mutations:
                assert production.count(old) == 1
                mutant = record / (name + "-production.c")
                mutant.write_text(production.replace(old, new))
                harness = record / (name + "-harness.c")
                original_include = '#include "../../../../drivers/misc/ws73/hcc.c"'
                harness_text = (HERE / "hcc.c").read_text()
                assert harness_text.count(original_include) == 1
                harness.write_text(harness_text.replace(original_include, '#include "' + str(mutant) + '"'))
                run(name, harness, function, "x86_64", contract)
            assert not git("status", "--porcelain") and git("rev-parse", "HEAD") == manifest["source"]
            assert all(sha(SOURCE / n) == h for n, h in manifest["sources"].items())
            manifest["status"] = "BOUNDED_PASS"
        except BaseException as error:
            manifest["status"] = "NOT_PROVEN"
            manifest["error"] = repr(error)
            raise
        finally:
            manifest["artifacts"] = {str(p.relative_to(record)): sha(p)
                for p in sorted(record.rglob("*")) if p.is_file() and p.name != "manifest.json"}
            save()
        print(manifest["status"], flush=True)


if __name__ == "__main__":
    main()
