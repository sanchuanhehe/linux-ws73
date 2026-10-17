.. SPDX-License-Identifier: GPL-2.0-only

WS73 codec qualification: 2026-10-17
====================================

Tested source and scope
-----------------------

The compiled/tested source is **719483121c0d3d10eccbd7a2cc988416d4f26d00**,
with a clean worktree before and after the run. Its parent codec change is
975062e43d1cd96da676b8fd932428b1554c2bb9.
The report commit adds documentation and a Kconfig help sentence only;
it is not the source identity of the compiled kernel.

This establishes executable BLE codec behavior in a synthetic VM. It does
not establish a USB driver, firmware loader, hci_dev registration, hardware
command/event correlation, radio communication or recovery. No physical
device, firmware or board file was supplied to this VM. No BLE peer is
available, as confirmed by the user. No controlled Wi-Fi AP is configured.

Initial branch/worktree status was clean at
70dd4ee72eb2d5793c62e71c14e3aba78e5a4e56. The personal remote matched.
Read-only GitHub comparison found torvalds/master at
58161ead29e50d70a317ea26b472e0c7508e69da, 80 commits ahead of that base.
The branch was not rebased and no shared remote configuration was changed.

Build and VM
------------

The command, from the independent ws73-upstream worktree, was::

   python3 tools/testing/ws73/run-kunit.py --jobs 4

Tools: Python 3.14.7, GNU Make 4.4.1, Clang/LLD 22.1.8 and QEMU 10.2.2.
The manifest records executable paths, hashes (including real compiler
candidates behind the ccache shim) and qboot ROM hash. CCACHE_DISABLE=1,
LLVM=1 and W=1 were used. The fixed O= directory was
``.dev/ws73-upstream/build-kunit`` outside the kernel worktree.

At build admission, no other compiler/build processes were observed;
the SparkLink hardware VM was active, so this project reserved four jobs
within the eight-job coordinated budget. No main-project process was
stopped or modified. The test VM used two vCPUs, 1 GiB RAM, a private QMP
socket, no NIC, no disk/rootfs and no USB passthrough. The actual QEMU
command and complete serial output are retained.

KASAN_GENERIC, KASAN_INLINE, UBSAN, UBSAN_BOUNDS, KUNIT and
BT_WS73_KUNIT_TEST were enabled. The codec object contains ASAN load/store
and UBSAN out-of-bounds hooks; all ten test cases executed that object.
Ten cases passed, none failed or skipped. This is not formal verification
or a long-running fuzz result.

Artifact SHA-256 values::

   .config
   31d2c49f1091568084d26ebda6e186aa70a81b5418904f84cb29736cb5a6e4da
   bzImage
   d1a6f714505f5ec30329e10fbae04f64321fec34c2971a19b76e7c958f533f21
   vmlinux
   18e875c8db6d6d79080b2412923b9f5cb6bdb798998b5d2d09118b052e01db96
   btws73_proto.o
   fc2c93fb8e871235f962f7b1603ef38a10ee94c2a7d79a1eb6ea7c92db4aadfa
   btws73_test.o
   2973d5d795eabab7c1205bd4b25bdefaabcf61113445f0cb6b91879feae1ded5
   raw serial test.log
   0b0d1637a632a0ef72c139d4344b2897eff97199b01497e0e0e2d375a4cbff2d
   manifest.json
   076c85783557382e7ccc3c7112b3b3ea8c3635c599782f7925eb52e8cad72e3f

The local sealed record is under the workspace root at
``.dev/ws73-upstream/records/20261017T045430.478329Z/``. The timestamp is UTC;
this is 12:54 on 2026-10-17 in Asia/Singapore. Large artifacts remain local.
The manifest enumerates and hashes the command, patch, config, tool
identities, results, binaries and raw logs, including failed-run status.

Tests and preserved attempts
----------------------------

The ten cases cover 1..24 records, unaligned reads, every truncated prefix,
all 65536 payload length values, all incorrect byte-sized service/queue
identities, full-width outer IDs, empty/gapped/oversize aggregates, maximum
record budgets, HCI direction/type/length errors, exact TX bytes, capacity
limits, null arguments and unchanged outputs on errors.
Synthetic event bytes do not demonstrate receipt of a real HCI event.

Earlier attempts remain under ``records/20261017-baseline``:

* ``kunit-initial.log``: configuration rejected nonexistent
  CONFIG_PANIC_ON_WARN; replaced by the panic_on_warn=1 kernel argument.
* ``kunit-v2.log``: invocation failed from the wrong directory, no build.
* ``kunit-v3.log`` and ``kunit-v3-serial.log``: kernel built, but the sandbox
  denied QMP socket binding; zero tests ran. This is an infrastructure failure,
  not a passing test run or a device fault.
* ``checkpatch-initial.log``: one alignment check, subsequently fixed.
* ``checkpatch-series.log``: a short-help warning, subsequently expanded;
  a new-file MAINTAINERS reminder (existing Bluetooth pattern covers the
  codec); and a duplicate-signature warning caused by feeding two mail
  patches to one invocation. Individual patch checks are kept separately.

Unchanged-base W=1 kernel-doc warnings in printk, iomap, block and
rhashtable are recorded in kunit-v3.log. No warning was emitted for the
new codec/test objects. Two intermediate dirty-source VM runs also passed:
``records/20261017T045025.190529Z`` and ``records/20261017T045250.535993Z``.
They do not replace the clean-source record above.

Original firmware and reference audit
-------------------------------------

The active selection is the three separate original 110 files listed in
ws73.rst. The file checker passed their sizes, internal SHA-256 headers and
pinned whole-file identities. Six negative checks rejected a missing file,
truncation, corrupted payload, modified-and-rehashed payload, wrong role
and a combined-111 file substituted for the original code file. The
combined file appears only as a deliberately invalid offline input.
This verifies file identity/integrity, not firmware execution, calibration
compatibility, chip revision or authenticity.

The local negative-check tool is ``.dev/ws73-upstream/firmware-negative-checks.py``;
its hash and the checker hash are in
``records/20261017-baseline/firmware-negative-final.json`` (SHA-256
fffbf82aedced95874d5cc73a6cc87c8b621021362668b48be245b16b33841c1).
Reference commits, dirty states and 35 inspected source/license hashes
are in references.json and reference-files.json. Firmware inventory and
the selected-original-file report are separate from VM qualification.
No firmware or calibration contents are committed to this branch.

Hardware and outstanding work
-----------------------------

``records/20261017-baseline/usb-baseline.json`` retains the allocated
1-1.1.1 device's serial, descriptors and ancestry under 1-1.1. The baseline
used passive sysfs reads; the interface was unbound. No hardware ownership
transfer, initialization, firmware write, reset, unbind or power operation
was performed. Runtime identity, speed changes and actual device ownership
must be rechecked before the next hardware action.

UP-02/04/06 now have a bounded codec implementation, not a functioning
transport or HCI adapter. UP-11 has one x86-64 instrumented VM configuration,
not the full BLE/Wi-Fi/module/architecture matrix. UP-01/12 retain firmware
delivery and hardware compatibility work. Wi-Fi MAC audit is preliminary;
the Host authentication/association and reorder evidence argues for first
evaluating mac80211, but does not resolve TX status and offload gaps.
All existing issues remain open. UP-05 remains deferred and non-blocking.
