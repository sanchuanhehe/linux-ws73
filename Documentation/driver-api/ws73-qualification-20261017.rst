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

HCC identity and WS73E follow-up
------------------------------

The next compiled source was **77c3956bf0292317e8e1e711b28a4716de3918c8**, clean before and after execution.
All 13 named cases passed under the same x86-64 KASAN/UBSAN configuration.
The added cases cover query bytes/padding/capacity, all U/S/E and bus wire
values, unaligned replies, every truncated prefix, independently malformed
header/status/identity fields, null arguments and unchanged error outputs.
These are synthetic protocol fixtures, not live device identification.

The record is ``.dev/ws73-upstream/records/20261017T050342.933647Z/``.
The runner command remained ``python3 tools/testing/ws73/run-kunit.py --jobs 4``.
Four build jobs were admitted after checking host load; the main hardware
VM was left untouched. No USB device was supplied to the test VM.
Artifact SHA-256 values::

   .config
   31d2c49f1091568084d26ebda6e186aa70a81b5418904f84cb29736cb5a6e4da
   arch/x86/boot/bzImage
   694a221bfc56098158417bd7e0027ad49f9e1dfe02c16fe7305b5a21f63e75bf
   vmlinux
   e6e874b98bd34852caa169cdf4ef18b11d45999c1b20f3aa444028ed1fb5caa9
   test.log
   b8b7d4948a050baebdfd0cf9fda0c631fd069e2e3686bcd48c8aa095dc719741
   drivers/bluetooth/btws73_proto.o
   a58aec7d63757d7f218b66a20dcf2ecf784b43d99d9d2109fb9076da70713184
   drivers/bluetooth/btws73_test.o
   e09006ad52272688dabf1ce06afa879aa20823b5da08a301a56fd08ccc548475
   manifest.json
   8dc333c7765544d2a9e628e1a2f8fa3d8c5d9fe4758939cb6e9150f3222ee995

Both pinned original firmware profiles (us and e) passed file validation.
The six earlier corruption/missing/wrong-role cases were rerun successfully;
eight additional checks rejected both cross-profile sets and each mixed
code/Wi-Fi/BTC file. This does not establish E hardware support. Reports are
``records/firmware-{us,e}-identity.json``, ``firmware-negative-identity.json``
and ``firmware-profile-negatives.json`` under the same private project root.
The ten SDK loading/identity source hashes are retained separately in
``records/sdk-loading-identity-files.json``. No binary contents were published.

Strict file checks of the three C/header files had zero errors/warnings.
Individual commit checks retained new-file MAINTAINERS reminders, the earlier
short Kconfig help (fixed in the report commit), one historical unwrapped
commit paragraph and a documentation spelling warning (fixed subsequently).
No checkpatch error was reported. These checks do not constitute upstream
review. The SDK loading audit and preference for drivers/misc/ws73 are
provisional design evidence; transport, loader, HCI and cleanup integration
remain unimplemented. Cold-start HCC identity availability is still an open
hardware/protocol gate, so no guessed U/E firmware was loaded.

Shared-core, boot transactions and H4 stream increment
------------------------------------------------------

The shared HCC migration retained all 13 cases unchanged. Its clean-source
run at 92f2e06c7800 is records/20261017T053022.087868Z. Firmware/profile
validation and bounded boot transactions added five cases; the clean
18-case run at 18f290020cb6 is records/20261017T053705.562167Z. An earlier
dirty 18-case run is retained at records/20261017T053533.150180Z.
All record paths here are under .dev/ws73-upstream/.

Subsequent SDK/libws73 comparison found a faulty QUIT fixture: the first
executor and its test both omitted the command's trailing space. Those
passes do not establish the correct QUIT wire bytes. Commit 9b9ee2ec18d5
corrects both to the SDK's five-byte command; no physical download had run.
The earlier records are retained, not rewritten as corrected results.

A separate core-only VM built with CONFIG_NET disabled and no Bluetooth
passed all five shared-core cases at 8f0e0c44b402, then again at the corrected
9b9ee2ec18d5. The records are 20261017T054204.019185Z and
20261017T054428.012737Z. The command adds --core-only to the normal runner.
The runner checks the resulting .config and expects exactly the core suite;
stale Bluetooth objects in the reusable build directory are not evidence.
This establishes build/runtime independence for these helpers, not a
BLE-only hardware configuration or the future module-dependency matrix.

The latest full instrumented VM run compiled clean source
**b1b1729259d3f9a1af81f30f9d0eac2fa640d4f5** and passed all **21 cases**.
It is records/20261017T054924.481201Z, run with::

   python3 tools/testing/ws73/run-kunit.py --jobs 4

The new H4 cases cover every two-part split of maximum event/ACL packets,
coalesced and bytewise input, zero-length payloads, all type bytes and all
65536 ACL length values, permanent failure after malformed input, explicit
reset, and argument rejection. Firmware tests use synthetic file bytes
with production SHA-256 validation; boot tests use a scripted transport,
including errors/short I/O/timeout/cancellation at every I/O boundary.
They do not exercise request_firmware deployment, USB/URB lifetime, board
preparation, real identity, firmware execution, hci_dev or any peer.

KASAN/UBSAN were configured in x86-64 QEMU with two vCPUs, no network, USB
or disk. The retained object inspection in h4-fw-boot-instrumentation.json
finds KASAN hooks in H4, firmware and boot objects, and UBSAN hooks in H4.
The absence of a diagnostic is limited to the paths these tests execute.
KCSAN, lockdep, fuzzing, formal verification and other architectures remain
separate incomplete gates.

Latest artifact SHA-256 values::

   .config
   7a981d0075072e8f9c5fb78c728f36b48943495309562be6fe1435045f08fedf
   arch/x86/boot/bzImage
   fa80e696cdbbf6cc3883efaca2923c31d8d9b5e7fc7dfb340a3d9d9144904d1f
   test.log
   08260e8664455f8cd81f44877a52d8fac598e59d456139f60afb2f94665b0294
   drivers/bluetooth/btws73_proto.o
   570be25f905bef40eb18a79ff0732395ad53efebc953111c2a70eddbc5ae033e
   drivers/misc/ws73/firmware.o
   42ab055d6a44d37418a02b9e1ed565bbaf06fb0ef98d792faa562c7270067cc1
   drivers/misc/ws73/boot.o
   3f960bd4bd1e962350a08d2095be6b7ee6473590ccca42ccc04459397464a9cf
   manifest.json
   e6067217f35a493bf4bf382336567bd94f89a8fceb5658c87725abcd4973858d

The first H4 build failed before execution because KUNIT_EXPECT_PTR_EQ
received an array expression instead of an explicit pointer. Its FAIL
record 20261017T054651.587538Z and original unpublished source patch
failed-h4-source-65708ff852c1.patch are retained; the unpublished commit
was corrected before pushing. The patch reconstructs that source from
9b9ee2ec18d5. No published history was rewritten. The corrected clean run
above supersedes its qualification, not its failure history. Initial
checkpatch alignment findings and corrected zero-finding reports are also
retained. Existing unrelated W=1 base warnings remain in build logs.

The libws73 identity follow-up is pinned in
identity-libws73-followup.json. It confirms a boot register-access mechanism
but not the missing eFuse-to-U/E mapping or status bitfield definitions;
see ws73-roadmap.rst for the exact source paths and limitations. SDK local
use remains authorized. No vendor source or binary was published.

Retained failure patch SHA-256::

   f83d3ed757fe5592427e5220a4788e799f6fb9fe040378a96f4e34c100a61fb0

Fixed boot identity query and first project-owned USB observation
----------------------------------------------------------------

Compiled clean source **cf8fc9628899b16daa10cbeed69b27237d60b12b** adds the
fixed boot READM decoder/transaction and an initial USB parent. All **24**
KUnit cases passed in records/20261017T060557.135517Z with KASAN/UBSAN.
The three new cases cover every low-16-bit value, upper-bit invariance,
all reply counts 0--513, zero U/USB, little-endian raw preservation, short
command/reply, transport failure, timeout/cancellation at both I/O boundaries,
and no output publication or further I/O after a failure.

This project's independent static audit is records/rom-identity-static-v1/.
Both original firmware identities and internal digests were rechecked, the
56-byte functions extracted using payload-relative load addresses, and the
Hisi tool rerun. The private audit retains file/function/tool hashes and
output. It confirms the supplied mapping, not cold-start register validity.
No vendor code, extracted firmware bytes or disassembly is committed here.

A separate, NET-disabled observation kernel from the SAME compiled source
uses built-in WS73 USB/xHCI, usbmon, KASAN/UBSAN and an initramfs observer.
The private build directory is .dev/ws73-upstream/build-observe, protected
by the project build lock; LLVM/W=1 and four jobs were used. The observer
has no shell or device-write operation: it mounts proc/sysfs/debugfs and
prints usbmon records. QEMU has two vCPUs, 1 GiB, no disk or NIC. A no-device
boot passed at records/usb-observe-20261017T061439.193641Z before hardware
was attached in a new VM.

The physical observation record is
records/usb-observe-20261017T061500.347386Z. Before attachment, host sysfs,
serial, VID/PID, hub ancestry, descriptors, driver absence, node permissions
and fuser were checked. Only assigned 1-1.1.1 was passed through by host bus
and port plus VID/PID. No hub or PCI-controller passthrough/reset was used.
The exact USB traffic observed in the guest was:

* OUT endpoint 1: submit and successful completion of 19 bytes,
  ``READM 0x40019000 4 `` (including the trailing space).
* IN endpoint 0x81: requested 512 bytes, successful completion of exactly
  four bytes ``00000000``. These are captured USB bytes, not an assumed
  zero-filled receive buffer.
* The driver logged raw 0x00000000, package 0, bus 0: U/USB.

There was one bulk OUT command and one bulk IN response. No WRITEM, FILES,
firmware, calibration, eFuse control or HCI request was sent. Standard USB
passthrough/enumeration can reset the USB device and is not a power-cycle
qualification. The device's power history was UNKNOWN. The result is
**OBSERVATION_PASS_NOT_COLD**, not automatic-selection readiness. No result
from the separate SparkLink U/E trials is counted as this project's pass.

QMP removal produced a guest USB disconnect; the VM then exited with status
zero. Post-exit serial, topology and descriptors matched the pre-attachment
record, no host driver or usbfs owner remained, and no KASAN/UBSAN warning
was found. This covers normal read completion and removal, not every URB
race, allocation failure, module unload or natural recovery scenario.
Cold-power U/E starts, other samples, read-validity conditions and runtime
HCC comparison after this project's own firmware loading remain unexecuted.

The local runner and initramfs sources, loader/libc hashes, binary, kernel,
objects, configuration, command, QMP exchange and usbmon/kernel output are
copied or hashed in the record. A separate immutable build proof binds later
observations to the compiled source rather than a documentation HEAD.
The trace verifier rejected 13 synthetic negative controls (short/long or
failed transfers, wrong command/device/endpoint, ordering, absent data and
duplicate records); two zero/nonzero positive fixtures passed. This limited
observer check does not complete the full qualification-supervisor project.

Preparation failures are retained: the first static init link lacked static
libc (original output is in the tool conversation; the local failure note
is explicitly retrospective), and the first VM preflight rejected missing
PCI/xHCI support before launching QEMU. The latter is
records/usb-observe-20261017T061108.177916Z. Corrected configuration used a
new build log and new records; no hardware was attached during those failures.

Artifact SHA-256 values for the physical observation::

   .config
   2937361d6190dec589c5bb1ebb01c83aa0cc64668638a193c1128c3fe988f6b1
   bzImage
   bc60e043c50c6dd55da742efe2ec8b26d8e3b714c24fe432a4e64efc31ee0947
   usb.o
   cb06d3c36ce066e20681190a497bc44d31e8ecd271de51d99cd11769d73eaef0
   boot.o
   b5cfec1e70b36bdfc7cc16d28d3dbae33ede411294f3af250d2c4b28f1cfb907
   initramfs.cpio.gz
   fe2a30a49b5c2e1b8e90133552805b2f0839a8ceed3e5e21dfdbe5ca412a9f80
   run.py
   a4128feff521c1cc9f635f314af3e719012ffdcde1f52d77b3f17f7b5fb0efae
   kernel.log
   08909edfd93a91fc7996962b0b463ed0ad4ebd6665dcedab56e176be7c41870d
   manifest.json
   4570495754abaeb4662e4fc6deb8180bf3878d39b2d0cbf7b97eec71f95b3f22

24-case KUnit manifest SHA-256::

   9c31a1d8c519f5f6d55f00e6e065b181e8b54f16db083bdae71e1be20f43aa53

Runtime USB wire and bounded sender increment
---------------------------------------------

Signed implementation commits ``ecc305265e3b`` and ``196d9f407ee6`` add
runtime single-record/control encoding, complete aggregate validation for
queues 7/9, and per-instance synchronous transmit sequencing. The USB parent
still performs only boot observation; no runtime frame is sent to hardware.

The full 32-case x86-64 KASAN/UBSAN VM run at
``records/20261017T063017.538974Z`` passed on clean compiled source
``196d9f407ee6bcf74e7909d4eca7805744c79e71``. Its manifest SHA-256 is::

   0411d1b7d78937dfd2967bb7e23c23d1f985dc51ba1da6c7e0b51313325ce5ed

The eight new cases exercise production envelope parsers/encoders and the
sender: full control/data byte fixtures, all shorter capacities, all input
lengths through the 4096-byte record bound, all 16-bit platform lengths,
unaligned maximum aggregates, every truncation, high queue bits, inconsistent
final records and unchanged outputs on errors. Sender tests cover initial
switch/reset/data, same-queue reuse, switching, two independent instances,
sequence wrap, and short/oversized/error/timeout/cancel at every transfer.
Failed sessions reject subsequent sends even after an external fault clears.
Invalid records cause no transport call. These tests do not establish
concurrent URB teardown, device credit, platform transaction matching,
firmware initialization or Bluetooth operation.

Two earlier wire-only attempts used ``1087f0387402`` plus the archived dirty
patch SHA-256 ``f45fe063f6e490687cd54e7ec2611591c53a8d4cab86615e9093cdd9445ccecc``:

* ``20261017T062658.512512Z``: FAIL; compilation succeeded but QEMU could not
  bind its private QMP socket inside the sandbox. No KTAP cases ran. The
  supervisor rejected the missing results rather than declaring success.
* ``20261017T062727.575889Z``: PASS, 29 cases, after running the same source
  in the authorized VM environment. This dirty-source qualification is
  recorded separately from the later clean 32-case result.

All attempts retain source patches, configurations, compiled objects, VM
logs, results and manifests under the independent private records directory.
Both code increments passed diff whitespace checking and checkpatch with
zero errors; each has the generic new-file MAINTAINERS reminder. A complete
maintainer entry remains part of the actual-consumer submission series.

Integration follow-up from SDK110: bulk data whose size is a multiple of
endpoint maximum packet size needs the SDK's ZLP semantics in the actual
USB adapter. BLE open/close uses class/interface USB control request 0,
value/index 0, four-byte message ID 25/26; it is separate from the queue-9
platform HCC request path. Neither is wired to a physical runtime instance
in this increment. Actual enable replies/status, tokenless reply quarantine,
continuous RX and auxiliary/HCI consumer lifetimes remain implementation
work, not passed gates. Cold-start U/E validity and hardware resources remain
as recorded in the previous boot-observation section.

The separate core-only run ``records/20261017T063058.444541Z`` passed all
16 shared cases on the same clean compiled source, with NET disabled and
Bluetooth/USB-driver configuration absent. Its manifest SHA-256 is::

   ca5d0c3b9e3ce21f0a7b663dc5c52d5dea3619bb7f4028d00d16798a597142d6

The full and core-only runs use separate saved artifacts despite sharing the
fixed incremental build directory. This confirms the current helpers build
without Bluetooth; it does not qualify future BLE/Wi-Fi module combinations.

Runtime identity and synthetic USB integration
---------------------------------------------

Signed production commit ``4dae17439a45`` adds a one-shot GET_CHIP_TYPE waiter
with independent RX completion, terminal abort and no request reuse. The USB
parent consumes it on a validated five-endpoint runtime interface. The boot
READM gate remains unchanged. No firmware download or function registration
is enabled; a runtime observation is not a verified firmware epoch or version.

Clean compiled source ``4dae17439a45`` passed 37 KUnit cases in the independent
x86-64 KASAN/UBSAN VM, record ``20261017T064546.582441Z``. The five new cases
cover replies before local TX completion, unrelated IDs, repeated/late replies,
error/malformed replies, TX failure after a reply, pre-admission removal,
abort during queue control, one-shot admission and a separate kernel RX thread.
The manifest SHA-256 is::

   048e0f6d3ea87f973fb9c0471e890b5951fab7c77bb71e1469ae851c5d41fd5e

A separate hardware-free VM kernel from the same clean source was built with
NET/Bluetooth/KUnit disabled, and WS73_USB, dummy_hcd, FunctionFS, KASAN,
UBSAN, PROVE_LOCKING, DEBUG_MUTEXES, DEBUG_SPINLOCK and DEBUG_ATOMIC_SLEEP
enabled. Its build record is ``virtual-usb-build-20261017T064921.437287Z``;
manifest SHA-256::

   f6dc989c23e5486ed025df8a34b41aa77af689e3179d3a17f045f59504118e2e

The newly written guest PID 1 constructs a synthetic five-endpoint USB device
inside that VM, checks all bytes of switch/reset/query OUT transfers and
supplies synthetic platform replies. It binds/unbinds five fresh instances:
package E/USB reply, package U/USB reply, device error status, truncated
aggregate, and disconnect while awaiting the identity reply. These package
values are fixtures, not observations of U/E hardware or firmware execution.
There are no physical USB devices, host controller passthrough, network or
disks in the VM command. Only the guest's dummy UDC is permitted by the fixture.

Both final runs passed all five cases, each observing 15 exact OUT transfers,
four IN replies and five disconnects. The full-speed 128-byte data transfer
terminates a 512-byte receive request through the ZLP path; high-speed uses
the same bytes with its different maximum packet size. Runtime state and USB
sequence restart for each new instance. Kernel and fixture reports use separate
serial ports; the fixture records the actual /dev/kmsg events before declaring
each case complete, avoiding printk delivery-order assumptions.

* Full-speed: ``virtual-usb-20261017T065533.532982Z``, manifest SHA-256
  ``b39a7dd76d6b191cbc3f94a0e6c514a9d9bc0128a50ec59780c141e3d536e334``.
* High-speed: ``virtual-usb-20261017T065626.165023Z``, manifest SHA-256
  ``9ddaac292285802e6515357a0407c51b0bdbdf3358a0956fc5e01d0c420994cf``.

All earlier attempts are retained:

* ``virtual-usb-20261017T065208.162062Z`` failed fixture setup: configfs did
  not resolve the relative function symlink. Switching to an absolute guest
  configfs path fixed the fixture; no driver change was made.
* ``virtual-usb-20261017T065230.384949Z`` failed the removal expectation.
  USB core delivered -ESHUTDOWN before disconnect's -ENODEV. Preserving that
  first terminal error is intentional; the fixture now accepts those two
  removal orderings, without accepting unrelated errors.
* ``virtual-usb-20261017T065400.844408Z`` passed with shared-console reporting,
  but printk and fixture output could interleave. The final two-port runs
  replace it as the preferred trace evidence, without deleting it.

The final supervisor was checked against an actual passing report plus 15
negative controls: missing/duplicate overall result, missing case, short/extra
TX, missing/wrong RX, missing disconnect, KASAN/UBSAN/lockdep report, wrong
identity/removal errno, missing captured kernel record and wrong USB speed.
The result is ``virtual-usb-supervisor-negative-v2.json``; it qualifies the
text verifier's rejection behavior, not additional transport behavior.

The private ``.dev/ws73-upstream/virtual-usb-tools/`` harness and each record
preserve helper C, runner/build scripts, compiler commands, libc/loader hashes,
initramfs, exact kernel/configuration, tool hashes and both raw logs. Promotion
of this prototype harness into a portable in-tree test entry point is pending;
these are local reproducible records, not a claim of published CI coverage.

No sanitizer, lockdep or atomic-sleep report occurred in the qualified runs.
Allocation failure, module unload, all callback races, interrupt notifications,
register endpoints, arbitrary control transaction reuse, version/boot identity
agreement, firmware loading, auxiliary/HCI consumers and peer traffic remain
unqualified. This increment operated no physical WS73 device and does not
change the previous OBSERVATION_PASS_NOT_COLD hardware result.

Serialized chip/version query increment
--------------------------------------

Commit ``dbb9b31172a4`` mechanically renames the runtime request machinery
for its platform role. The unchanged 37 KUnit cases pass on that clean source
in ``20261017T070321.292178Z`` (manifest SHA-256
``bb0a9e75f14598793a2f1cdc37d5cd4b3583c1834e9873f041c40c340d6322f9``).

Production commit ``c189384d3996`` then adds version encoding/decoding and
serialized one-shot chip/version requests. A busy caller cannot start a
second request. Each reply kind is used once per instance, a late chip reply
cannot satisfy the version request, and any request failure prevents further
platform queries. The USB initialization worker carries a single absolute
deadline across both stages and logs observations only after both succeed.
The raw firmware/ROM words are not a firmware compatibility approval.

The clean source passes 40 KUnit cases in ``20261017T070730.183072Z``.
New cases cover exact request bytes, all shorter output capacities, unaligned
reply words, every truncation, all 16-bit declared lengths, error status,
wrong lengths/IDs, raw zero/unknown versions, concurrent admission, replies
for a different stage, failure after the first successful stage and forbidden
reuse. Manifest SHA-256::

   986d411bd085a271e552b98f458ec7e555b55e6fe90b97f30b376f09eb4e4a67

The separate KASAN/UBSAN/lockdep/DEBUG_ATOMIC_SLEEP virtual USB kernel is
from the same clean production source, build record
``virtual-usb-build-20261017T070901.015159Z``, manifest SHA-256::

   84bea31ed1b0027855761f20a625f2d7b4af26714d3c6fc2f1b4d696e349deca

Both speeds pass eight synthetic FunctionFS/dummy_hcd cases: two successful
chip/version combinations, chip error, truncated chip aggregate, removal
while waiting, version error, wrong version return length and a stale chip
reply followed by the valid version reply. Each run checks 29 exact OUT
transfers, 13 IN replies and eight removals. Queue switching is not repeated
between the two platform requests, and the USB sequence advances to 3 for
the version request. All identities/versions are synthetic fixture values.

* Full-speed: ``virtual-usb-20261017T070939.593645Z``, manifest SHA-256
  ``cddfc7133f3916a281a1a156d26cb49e561b4e4ff7690763203f4bcab4d5b348``.
* High-speed: ``virtual-usb-20261017T071056.412119Z``, manifest SHA-256
  ``565e0fb58212f4cfebdc2837f8668b361fbb15557eb4826bc35be6e7486e5992``.

The updated text supervisor rejects 19 negative controls, recorded in
``virtual-usb-supervisor-negative-v3.json`` with its exact verifier hash.
In addition to the previous controls, these include a missing version TX,
wrong raw version, wrong reply role and missing stale-reply injection.
No sanitizer, lockdep or atomic-sleep error was reported. The original and
updated fixture/runner sources are preserved separately with their records.
The prototype harness remains private and still needs a portable in-tree
entry point; no public CI or physical USB qualification is claimed.

The accompanying BLE initialization audit identifies customization/status,
boot status, two SDK enable call sites and vendor address command completion
as distinct requirements. None is implemented or qualified by version-query
success. Firmware execution, boot/runtime compatibility, cold-start evidence,
auxiliary/HCI consumers, E hardware, BLE peer/AP testing and formal submission
remain open. No physical device was operated in this increment.
