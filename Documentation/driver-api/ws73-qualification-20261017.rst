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

Function-control notification increment
--------------------------------------

Production source ``30cb2bfb50773d438b8ac481c6b9379074990ffd`` adds bounded
service-5/queue-10 RX notification decoding and USB parent observation. Every
record is validated before any notification is published. Unknown tags and
SLE actions reject the whole aggregate as unsupported; malformed known status
records enter the existing USB fault/drain path and wake a pending query with
its failure. Observation is bounded to four first-seen types per USB instance,
not a BLE enable completion or readiness state. Queue-10 TX remains closed.

The initial implementation exposed two evidence errors during reference review:
service 5 was initially encoded as 0x05 rather than 0x50, and Host TX's
allocation-size convention was incorrectly applied to RX. The pinned SDK's
actual ``hcc_header`` bitfields, libws73's documented wire correction and RX
walker, and the existing SDK110 status audit distinguish these conventions.
Commits ``744c8915323f`` and ``30cb2bfb5077`` correct them without rewriting the
initial ``296739b42c7d`` history. The tests had shared the initial assumptions;
their PASS results therefore did not prove the protocol. They are preserved,
explicitly superseded, in ``notification-nibble-correction.json``:

* ``20261017T072512.104782Z``: 44 KUnit on the first implementation.
* ``virtual-usb-build-20261017T072614.843733Z`` and runs
  ``virtual-usb-20261017T072650.121878Z`` /
  ``virtual-usb-20261017T072731.400771Z``: first synthetic USB results.
* ``20261017T072902.795200Z``: 44 KUnit after the nibble correction, before
  correcting RX length units.
* ``virtual-usb-supervisor-negative-v4.json``: original 24 text rejection
  controls; they check log rejection, not protocol correctness.

The final source and fixture both use header byte 0x50 and RX payload length
excluding the four HCC bytes. The fixed-byte test and all 256 first-byte values
explicitly reject 0x05, wrong service and nonzero subtype. Other tests cover
unaligned input, all truncations, all 16-bit HCC/tag lengths, valid status/action
values, high-bit invalid values, padding, unchanged outputs on error, valid
input after error and 1..24-record aggregate publication. Queue-10 RX support
does not implicitly admit unchecked customization TX. All 44 KUnit pass.

The separate virtual USB kernel enables KASAN, UBSAN, lockdep and
DEBUG_ATOMIC_SLEEP, with networking/Bluetooth/KUnit disabled. Full-speed and
high-speed each pass 11 cases: the prior eight chip/version scenarios plus
unsupported/duplicate function notifications, malformed tag size, and a valid
first status followed by an invalid final status. Each run verifies 41 exact
OUT transfers, 22 IN replies and 11 removals. The successful notification case
publishes exactly four observations; the malformed aggregate publishes none.
The following final records all bind the same clean production source:

* ``20261017T073018.661091Z``, manifest SHA-256
  ``3d63ddd06eeb6edeca759506b423435b2711c72b1861132505c884a25903b8b7``.
* ``virtual-usb-build-20261017T073049.937645Z``, manifest SHA-256
  ``ad66ff0ddac699489130afa80aff2991f79bdec382b256d982073f9b06bfb529``.
* ``virtual-usb-20261017T073156.937282Z``, manifest SHA-256
  ``e151cb2ccbe9a0297aa5a9d4953d639130e6ca06ca31c4e07766fdddfdf36e6b``.
* ``virtual-usb-20261017T073216.716238Z``, manifest SHA-256
  ``2cfe234e89046729bf3716b944256b98b1c520fe816f94712bf4fd6ac4769d8f``.

``virtual-usb-supervisor-negative-v5.json`` pins the corrected positive record,
runner and checker hashes and rejects 24 negative controls. Notification
additions include wrong/missing/duplicate status, partial publication on a
malformed aggregate and missing unsupported-message injection. No sanitizer,
lockdep or atomic-sleep error was reported. This is synthetic transport
qualification; no physical device, firmware execution or HCI function was
qualified. The portable in-tree USB harness remains pending.

The separate ``ble-layout-audit-v1`` compiles assertions against the actual
pinned SDK header for x86-64, arm64, i386 and powerpc64, establishing 140-byte
customization layout and specific field offsets only. The first attempt failed
before compilation because ccache could not write its temporary directory;
``attempt-1-ccache-denied.json`` is preserved, and a CCACHE_DISABLE=1 rerun
passes all four targets. It is not a cross-architecture driver build or proof
of valid board/calibration/address values. SDK address-source auditing also
identifies its INI/eFuse/random fallback, which is not adopted as a device
Bluetooth public-address source.

The initial implementation's checkpatch reports only the generic new-file
MAINTAINERS reminder; the new files are style-clean and both corrections report
zero errors/warnings. A formal driver maintenance entry remains part of the
actual-consumer submission series. No physical hardware was touched in this
increment; cold U, E hardware, peer/AP, firmware loading, enable/disable request
completion, native HCI and formal upstream gates remain open.

Interrupt RX and fault/drain increment
-------------------------------------

Production commit ``07a693e8744c`` adds exact eight-byte interrupt decoding
and a second, independently submitted receive URB. It starts before the bulk
receiver and platform queries. Fault notifications close admission, wake the
platform waiter and schedule shared draining. Device/BCPU panic bits take
precedence over BSP_READY; packet-loss status fails conservatively. BSP, raw
pool words, unrelated bits and USB completion do not enable BLE or establish
device send credit. Every USB object owns its interrupt buffer and URB.

The production commit passes 46 KUnit cases in
``20261017T073658.372957Z`` (manifest SHA-256
``70b9a3ff29a02c5910a3c422b08831877f9161b44bf63cbb7018fcc5a2d62fa0``).
The two new cases cover unaligned little-endian words, every event/pool bit,
zero words, all short lengths, oversize/SIZE_MAX, null arguments and unchanged
error outputs. Existing codec, firmware, boot, USB and runtime tests remain.
Checkpatch reports zero errors and warnings for this implementation.

The first virtual USB attempt on that source, build
``virtual-usb-build-20261017T073757.406994Z`` (manifest SHA-256
``587e8ed062ba2a5a2437cae3cdf1050e83f9b288870e76e261797f5be918a751``),
fails in ``virtual-usb-20261017T073843.332075Z`` (manifest SHA-256
``2ae021064ea03ee335f3e8411d586bb7c9458fbf44124c504a9ecdcad9583547``).
The fixture accepted only -ENODEV/-ESHUTDOWN during its intentional unplug,
but dummy_hcd can return -EPROTO when the endpoint disappears before driver
disconnect joins the URB. Its ``dummy_timer`` missing-endpoint branch explicitly
returns that error. The USB disconnect announcement also precedes the actual
drain, allowing the old callback's diagnostic to appear in the next fixture
case. This failed attempt is retained; it is not hardware recovery evidence.

Commit ``a175a23eadd7`` adds one optional dynamic-debug message after joining
both receive URBs and initialization/stop work. It changes no protocol behavior.
The final virtual USB kernel enables DYNAMIC_DEBUG, KASAN, UBSAN, lockdep and
DEBUG_ATOMIC_SLEEP with NET/BT/KUnit disabled. The fixture waits for the actual
drain marker before starting another instance. Removal-related interrupt errors
are allowed only after the explicit removal injection and before that marker,
at most one per instance. The waiting-query unplug case accepts the documented
dummy_hcd -EPROTO ordering as well. Unrelated/pre-removal failures, absent drain
or a post-drain callback cannot pass. No driver fault is converted to success.

The final USB build and both speed runs bind clean source ``a175a23eadd7``;
the 46-case KUnit evidence above remains bound to ``07a693e8744c`` and is not
silently relabeled after the diagnostic-only addition:

* ``virtual-usb-build-20261017T074028.829018Z``, manifest SHA-256
  ``01632c3b3010f403bc591aa038a0ee3edecf8434b1cf0247c45a3678b32dd5ba``.
* ``virtual-usb-20261017T074315.674446Z``, manifest SHA-256
  ``2acea25406f8f3706b6e3f055c359a7c861abfad738c04f03db351db3c477d15``.
* ``virtual-usb-20261017T074351.558035Z``, manifest SHA-256
  ``70b6472f8dba54cb56b3e03dd964e1c9ce4c2341f3d32db9e36e67f88d42d65c``.

Full-speed and high-speed each pass 17 synthetic cases, verifying 65 exact
bulk OUT, 30 bulk IN, eight interrupt IN, 17 removals and 17 drains. The six
new cases cover zero/unrelated/duplicate BSP notifications with a raw pool
word, seven-byte interrupt truncation, device panic with BSP_READY, BCPU panic
with BSP_READY, high-priority loss with BSP_READY, and panic after both platform
queries completed. Fatal packets publish no BSP observation; pending queries
fail with the expected error. The two runs record respectively six and five
allowed interrupt errors during injected removal, consistent with competing
USB completion/disconnect ordering. All original cases continue to pass.

The updated supervisor first rejects 31 controls in
``virtual-usb-supervisor-negative-v6.json``. Final supervision also requires
the drain to be the last captured driver event in each case and rejects 32
controls in ``virtual-usb-supervisor-negative-v7.json``. Its checker and verifier
sources are retained as ``virtual-usb-supervisor-negative-v7.py`` and
``virtual-usb-verifier-v7.py``. ``virtual-usb-revalidation-v7.json`` binds both
original VM log hashes to the final verifier; this is log revalidation, not
an additional VM run. The original run manifests and verifier remain intact. New controls remove the drain/removal marker or interrupt,
alter pool/interrupt bits/fault status, or inject false BSP success on panic.
No sanitizer, lockdep or atomic-sleep failure was reported. These observations
do not exhaust all interleavings, allocation failures or native USB PM behavior.

No physical device, reset, firmware download or BLE control request occurred.
BSP observation remains separate from customization/action/HCI completion.
Cold U identity evidence, E/peer/AP resources, board-derived customization,
actual BLE request/waiters, firmware integration, auxiliary/HCI and Wi-Fi
consumers, portable USB test tooling and formal upstream submission remain open.

BLE action state machine and prepared USB backend
------------------------------------------------

Commit ``a61a278ef3d7`` implements the per-device action transaction engine,
registers its RX/abort side in the USB parent and adds five KUnit cases. An
unknown controller is not assumed closed. Successful open/close actions can
alternate, while one command mutex excludes concurrent callers and rejects
redundant stable-state requests. A transaction needs both exactly four bytes
of successful local transmission and one matching action reply, within a shared
absolute deadline capped to 2000 ms. Wrong/duplicate reply, short/long/failed TX,
timeout, cancellation or abort is terminal. A failed close does not publish
DISABLED, and a successful reply cannot hide a failed send. Ordinary boot/custom
status messages cannot satisfy the action waiter. Device failures abort it;
BLE action rejection alone does not reset an active sibling or stop shared RX.

That source passes 51 KUnit in ``20261017T075356.646006Z`` (manifest SHA-256
``0cb4d4dc4fa8388b1f0572a82ce202afc1ae507b502fd1da41e9c7818fe63f46``).
The new tests run 100 alternating synthetic actions, check exact LE32 25/26
commands, reject concurrent/redundant/unarmed close admission, retain first
terminal errors, exercise actual bounded reply timeout, inject late/opposite/
duplicate replies and test cancellation before and after submission. A separate
kernel RX thread completes a sleeping caller. Deadline tests include a distant
caller deadline, the 2000-ms cap and U64_MAX arithmetic. Other mock instances
retain their state. These are production-engine tests with injected IO, not
physical repetitions or same-chip BLE/Wi-Fi isolation qualification.

Commit ``6d618efe84caea6d3840c2c967f0ffa5cf732eec`` prepares the fixed USB
class/interface transport. It accepts messages 25/26 only, builds request 0,
request type 0x21, value/index 0 and four-byte data length, allocates setup/data
buffers, and shares URB admission, cancellation and completion joining with
existing bulk IO. **Current initialization does not call this backend.** The
control-URB branch is compiled but has not run in the virtual USB fixture or on
hardware. Caller registration, firmware/board/customization prerequisites and
auxiliary/HCI lifetime management remain necessary before invocation.

The final functional source passes 51 KUnit again in
``20261017T075609.537623Z`` (manifest SHA-256
``8216891586a4073fc9cf66f08fce416ae82d3ee388fd01b4db8d411bd1cf353c``).
The separate KASAN/UBSAN/lockdep/DEBUG_ATOMIC_SLEEP USB kernel and both speed
regressions below bind that same clean source:

* ``virtual-usb-build-20261017T075722.158160Z``, manifest SHA-256
  ``f2f58a97a478d7c1f5f49425eb9857fb73254201cc0be370a4a63f5dda84107b``.
* ``virtual-usb-20261017T075835.450667Z``, manifest SHA-256
  ``30d7452c210fb9887589d506d88489318511b2190bc766acaa177c7255020613``.
* ``virtual-usb-20261017T075917.129556Z``, manifest SHA-256
  ``c52016d59255752c91b7a57027dc1409d53543c91e5541096d7c59e2837fbbe4``.

Each speed passes the existing 17 cases (65 bulk OUT, 30 bulk IN, eight
interrupt IN, 17 removals/drains). They exercise unarmed notification routing,
shared fault abort and bulk/interrupt teardown after the transport refactor.
They do not exercise an armed BLE action or the new class-request branch.
The unchanged verifier hash matches the prior 32-negative-control qualification
in ``virtual-usb-supervisor-negative-v7.json``; those controls were not relabeled
as new executions. No sanitizer, lockdep or atomic-sleep error was reported.
The documentation increment also wraps one overlong assertion without changing
behavior; its HEAD is not substituted for the compiled source above. The final
new files are checkpatch-clean; the initial new-file MAINTAINERS reminder remains
pending the actual-consumer submission series.

Tokenless notifications still assume ordered one-reply-per-command device
behavior. The engine cannot authenticate an old same-kind reply delayed across
multiple successful action cycles. Synthetic success does not discharge that
hardware/epoch requirement or prove boot, customization, address or HCI setup.
There were no physical USB operations, BLE requests, firmware downloads or
external submissions. Cold U, E/peer/AP, board inputs, actual action invocation,
auxiliary/HCI, Wi-Fi and portable USB test tooling remain open.

Explicit customization codec and one-shot acknowledgement
--------------------------------------------------------

Commits ``7e529ca4ee61`` and ``79680da45703`` add a fresh, typed SDK110
customization encoder and strict queue-10 TX admission. Explicit fields are
written little endian, with separately checked 800-byte HCC and 140-byte tag
lengths. Frequency/reference and channel-boundary packing follow the audited
SDK fields; count is bounded to 0..8. Only use_flash=0 is supported, with zero
flash-derived fields and padding. No board defaults, MAC fallback, flash
reader, userspace ABI or SLE adapter are installed. Structural validity does
not establish board/RF suitability. SDK INI frequencies and the no-INI defaults
are documented as distinct sources, not interchangeable qualified profiles.

The first codec run retained in ``20261017T081225.618998Z`` failed one of 55
cases: the older notification suite still expected all queue-10 TX/control
encoding to return -EOPNOTSUPP. This was an obsolete admission expectation,
not a device observation. The correction keeps RX notification records
rejected as TX (-EMSGSIZE) while allowing queue-10 control framing. The second
run at ``79680da45703`` passes all 55 cases. The failed run is retained.

Commit ``0df10f52427efb6e3f331bb93a8f2cdf9357d61d`` adds one-shot apply:
PREPARING during queue controls, WAITING only immediately before data submit,
then REPLIED and DONE only after both exact TX and timely CUSTOMIZE_RECEIVED.
Premature/duplicate/late status, failed/short/oversized TX, timeout, abort or
cancellation is terminal. One absolute deadline is capped to 2000 ms, including
queue controls and reply waiting. Unarmed observations are ignored, other
notification kinds cannot complete this request, and success cannot be reused.
The USB parent routes validated replies and shared aborts to the new object.

That final functional source passes all 60 KUnit cases. The nine added tests
cover an independent golden endian vector, every smaller output capacity,
truncation and forbidden header/tail changes, unchanged output on error,
strict TX admission, short transport at each stage, one-shot/reentrant behavior,
missing/premature/duplicate/late acknowledgement, cancellation, first-error
retention, U64_MAX deadline arithmetic and independent RX-thread completion.
The golden fields are deliberately synthetic and unsuitable as an RF profile.

Evidence manifests (SHA-256):

* ``20261017T081225.618998Z`` (FAIL),
  ``ec5ddd55c3e3b734662a44c00c0b5329ee9fe5f187d263c08ed31749674179a5``.
* ``20261017T081329.770688Z`` (PASS),
  ``8787513892f6f149b0b3244a1f7d7f0698e6ddcdb30ed5a8de1e16f17aec3bfe``.
* ``20261017T081755.008739Z`` (PASS),
  ``b37a67ed0a8ea2f5b4d7d6c95d738948a01b9846e027b6e6c8b60e77759045cb``.
* ``virtual-usb-build-20261017T081837.516896Z`` (PASS),
  ``4de1db981d93e95694ed97df4c536c3b51e9245401188b55b32ed6968f468f91``.
* ``virtual-usb-20261017T081913.980387Z`` (PASS),
  ``d02eeae5e7e3dd7d8352160a4fd6468ee79b7f4acc196e55fe8d99568823ad0b``.
* ``virtual-usb-20261017T081944.465822Z`` (PASS),
  ``5db8270428766feee1fbc7a92bfc80aa87014f8f205660eecbc3854643ddb5f4``.

The separate USB build and both speed runs bind the same clean final functional
source. Each speed passes the existing 17 cases: 65 bulk OUT, 30 bulk IN, eight
interrupt IN and 17 remove/drain sequences. KASAN/UBSAN/lockdep and
DEBUG_ATOMIC_SLEEP report no error. They cover unarmed customization status
routing and shared failure/teardown; they do not invoke customization apply or
the prepared BLE USB control backend. The unchanged USB verifier hash matches
its prior 32-negative-control qualification; those controls were not rerun or
relabeled as new executions. All six new C/header files pass strict checkpatch.

No physical USB operation, firmware download, on-device customization, HCI or
peer exchange occurred. A known firmware epoch and board-qualified parameter
source still gate actual apply, followed by BOOT/action/address/HCI setup.
Tokenless acknowledgements cannot authenticate an old epoch. A future caller
must be aborted and joined before parent release. Cold U confirmation, E/peer/AP
resources, three-file loading integration, actual auxiliary consumers, Wi-Fi,
portable USB test tooling and formal submission remain open. The documentation
commit does not replace the compiled source identity above.

Integrated startup handshakes
----------------------------

Functional source ``3886d506558c4f071484232e5575b1248c118baf`` adds the
per-device startup coordinator, owning the existing customization and BLE action
engines. One-shot arm precedes RX, while run follows independent firmware,
identity and board qualification. It requires BSP before customization, successful
customization TX/ack before BLE enable, actual enable TX/action, and BOOT before
DONE. BOOT can arrive earlier in the current armed session; it cannot substitute
for another stage. Status duplicates are idempotent, transaction duplicates retain
the existing terminal policy, and panic/loss overrides BSP in one IRQ word.

One absolute deadline starts at arm and is capped at 12000 ms. BSP/BOOT waits
have additional 6000/2000-ms caps, and customization/action retain their 2000-ms
caps. The SDK board wait is actually 1000 plus 5000 ms, not the nearby commented
20000; its status helper compares millisecond timestamps against 2000 while
sleeping 10 ms. The independent driver uses monotonic time. These bounds are host
policy, not measured hardware guarantees. Abort wakes status waiters and both
transaction engines without nesting the coordinator lock with their locks.
There is no uncertain-startup disable/reset/retry or successful Ready publication.

All 66 KUnit pass in the instrumented x86-64 VM. The six new cases call the real
coordinator, customization, bounded USB TX and BLE action engines with injected
IO. They check both BOOT orders, missing each handshake, failed/short TX, abort
between stages, IRQ fault priority, unarmed/repeated/reentrant admission,
cancellation, absolute expiry, U64_MAX arithmetic and independent RX completion.
In particular, absent BSP sends no data; failed customization sends no BLE open;
BOOT cannot hide a failed enable. Fixtures contain no usable board/RF profile.

The separate KASAN/UBSAN/lockdep/DEBUG_ATOMIC_SLEEP USB build and full/high-speed
regressions bind the same clean functional source. Each speed passes 17 cases,
with 65 bulk OUT, 30 bulk IN, eight interrupt IN and 17 removals/drains. These
exercise the parent's migrated notification/abort ownership with startup unarmed;
**they do not run the startup transaction or USB class-control backend**. The
unchanged verifier matches its previous 32-negative-control qualification; no
new negative executions are claimed. The three new files pass strict checkpatch.

Evidence manifests (SHA-256):

* ``20261017T082931.614158Z``,
  ``e01973bd239b1c07f9a932f0336ad00e3d4710882dbd17854197e72b8e5ab2bd``.
* ``virtual-usb-build-20261017T083006.661404Z``,
  ``86d404e34bbbc5dfc72fd2a4fa4baf44ee48e2d675e8cc3222fc008e1635eefd``.
* ``virtual-usb-20261017T083050.089035Z``,
  ``ee1ff168951d10d4cbc3115b755233fe3662f2a28ec9fddb33b54e38251b904f``.
* ``virtual-usb-20261017T083125.717461Z``,
  ``30961138e9eff1dbf47fbc053eae67872d88ec2b3fcdbb7afd6691828e4aa61b``.

USB initialization still does not arm/run the coordinator. DONE would mean only
BSP/customization/action/BOOT handshakes, not trusted firmware, board suitability,
address setup, actual HCI command completion or consumer registration. Those
preconditions and the eventual caller's stop/join contract remain required.
No physical device, firmware download, BLE peer exchange or external submission
was performed. Cold U evidence, E/peer/AP resources, board preparation/three-file
loader integration, auxiliary/HCI, Wi-Fi and upstream acceptance remain open.
The following documentation commit is not the compiled source identity.

Fixed board preparation before original image execution
-------------------------------------------------------

Source ``90b75a6d7d0ea6a7186e2a610fc7eb9cf97d04c4`` adds ``ws73_boot_load``. All three
image ranges and the explicit XO coarse/fine bounds are checked before any
write. The fixed RAM2/3/4 sequence and XO trim produce eleven WRITEM transactions,
each requiring exact successful TX and WRITEM OK (optional NUL padding only).
Only after all acknowledgements does it call the existing separate-image
FILES/QUIT executor. One caller-supplied absolute deadline covers the entire
sequence; per-transfer caps remain 2000 ms. Preparation acknowledgement count
is separate from image byte progress. Failure stops without retry, rollback or
continuation; the caller must discard that session. No generic register ABI,
board-default trim or firmware fallback was added.

The SDK supplies symbolic RAM operations/order/values, the boot WRITEM dialect,
XO address/width and configuration dependencies. Both inspected USB light configs
enable BTC EM-buffer calibration; the Linux platform Makefile defines the RAM2/3
preparation condition. Numeric RAM addresses match the pinned libws73 preload
implementation. The SDK checkout lacks the referenced generated CLDO offset
header, and a local corresponding object was not found. This mapping therefore
has reference-implementation evidence, not independently verified vendor headers
or project U/E hardware qualification. Static input hashes are retained in
``board-preparation-provenance.json``. Local firmware/board use remains distinct
from eventual redistribution and actual compatibility qualification.

The KASAN/UBSAN x86-64 VM passes all 69 KUnit in ``20261017T083953.234653Z``;
manifest SHA-256 ``fed131f81e51fe500ae950d698ea6e1625bcd41ca9e828af6b66bb5187f992e9``.
The three new cases verify a fixed 35-operation preparation/download transcript,
175 fault placements (five fault modes at every send/receive), and admission
rejection before side effects for invalid trim, wrong final image address,
missing final image, overlapping range, cancellation and expired deadline.
Progress is unchanged on invalid input; failed preparation never reaches FILES.
The existing hash/parser/chunk/startup tests also pass. New test source and the
boot implementation diff are strict-checkpatch clean.

These are synthetic immutable image views and a synthetic explicit trim, not
qualified board data or downloaded firmware. Full image/header hashing belongs
to the prerequisite firmware-request layer; range validity cannot replace it.
No USB parent invocation was added, and no physical operation or new USB fixture
run occurred. Firmware selection remains gated by live HCC/cold-read evidence,
board applicability and runtime identity/version checks. Runtime XO temperature
compensation, address provenance, native consumers and the remaining plan are
still open. The documentation commit does not replace the compiled source.

Bounded boot USB backend and reproducible virtual USB tools
---------------------------------------------------------

Functional source ``b2e1f7ee3c8da0834d3d43e05f7955bfd60846d2`` prepares a
persistent boot IO backend on the USB parent. Boot TX accepts at most 32768
bytes, boot RX at most 512, while runtime retains its 4160-byte envelope and
ZLP policy. Anchored synchronous transfers still check actual lengths and join
completion before releasing memory. READY delay is 5000--6000 microseconds;
the executor rechecks its absolute deadline and cancellation before payload TX.
The parent still only observes boot identity: it does not call the loader.
The new 32-KiB payload and READY delay paths are therefore prepared, not executed
by the USB regression. BSP timestamps now use the same boottime clock as the
other parent/coordinator timestamps, removing a mixed-clock deadline input.
This change does not establish suspend/resume support.

All 69 KUnit pass for this functional source in the instrumented x86-64 VM.
The hardware-free USB build, runner, fixture, negative checker and configuration
are now available under ``tools/testing/ws73/usb-vm/``. They build from an explicit
allnoconfig fragment without a private baseline archive, reserve four build jobs,
and run two-CPU Q35 guests with no host USB, disk or network. Clean source commit,
configuration, tool candidates (including the actual compiler behind ccache),
runtime libraries and artifacts are fingerprinted. Exact source/artifact checks
reject stale builds before launching a VM. Python optimization is rejected by
all three Python entry points because assertions are part of verification.

Two full-speed attempts failed and remain FAIL. The first guest completed all
fixture cases but lacked ACPI poweroff; the second enabled ACPI without PCI,
causing AE_BAD_PARAMETER during default region initialization and again leaving
poweroff unavailable. Each runner hit its 45-second deadline and killed/joined
only its own guest. Neither fixture PASS text was accepted as successful VM
completion. Source inspection identified the default PCI_Config region handler's
PCI dependency; adding PCI allowed normal guest shutdown. These are VM harness
configuration failures, not physical-device failures or PM qualification.

Final USB/tool source ``4031cc80f65295074c4ea192c50d25b2525828a7`` passes both
full- and high-speed runs under KASAN, UBSAN, lockdep and DEBUG_ATOMIC_SLEEP.
Each run checks 17 cases, 65 bulk OUT, 30 bulk IN, eight interrupt IN and 17
removals/drains, with normal QEMU exit. The fresh negative run rejects all 32
log corruptions and five invalid build manifests. Additional local receipts
``usb-stale-build-rejection.json`` and
``usb-tools-optimization-rejection-final.json`` retain the pre-launch rejection
and three optimized-entry-point rejection results.

All attempts below are retained; hashes refer to their manifest.json files:

* ``20261017T084611.158985Z`` (KUnit PASS),
  ``955e78efbf97ea1a8cf3f8436e6c6cab11ab20dd3944a834c243c278fbd3bd51``.
* ``virtual-usb-build-20261017T084947.302003Z`` (initial build PASS),
  ``5fae25f993eeb35f9ad766de682ad9e5d20ea60796a81bb8a1e2bf426dd8476d``.
* ``virtual-usb-build-20261017T085241.976759Z`` (provenance build PASS),
  ``125ded6f7f4a97be469ff4a8f2b992941bac8a0c42304a19ecc30f2e1124adcb``.
* ``virtual-usb-20261017T085312.790556Z`` (full-speed FAIL, no ACPI poweroff),
  ``617f4295477cde7d64a3059a26d9081215af408895bb8c55bc5a5c72f254e931``.
* ``virtual-usb-build-20261017T085424.628780Z`` (ACPI build PASS),
  ``1b1e2197d82a810a3dc9134856822ec6799ab48d1b37b350d32bda2391f50372``.
* ``virtual-usb-20261017T085624.601998Z`` (full-speed FAIL, missing PCI region handler),
  ``d332d6041ead82798b69841f7304b50c4e6f6d284341b547b018089ea6f96c8c``.
* ``virtual-usb-build-20261017T085758.617240Z`` (ACPI/PCI build PASS),
  ``ac26dffe9fe313dac7fd853ca931bd8c176d658727689325c892b5a0ea1ea875``.
* ``virtual-usb-20261017T090241.333881Z`` (full-speed PASS),
  ``10a50cf819d0f2e6d494cef98753d2acd6978ebc046180f33de8d345a52ae78c``.
* ``virtual-usb-20261017T090259.564553Z`` (high-speed PASS),
  ``5302138b2bc97aa48ef30724f5948fd2813876ffee1f483222f5098b05a77d6e``.
* ``virtual-usb-negative-20261017T090300.648508Z`` (32 log and five artifact controls PASS),
  ``6e0252f5dc3a65a3df99b3b52ca3e382d57aae2dab5f1ce2c27b2b1d75944915``.

The USB fixture still uses synthetic runtime devices and unarmed startup. No
physical operation, boot payload download, armed customization/startup, USB
class-control, HCI or Wi-Fi exchange occurred. The 69 KUnit and USB results bind
the distinct source commits above; subsequent documentation is not a newly
compiled source. Cold-start U/E evidence, trusted board/trim inputs, parent
loader integration, native consumers, peer/AP resources and upstream review
remain open. The goal and existing tracking issues remain active.

Boot READM through actual virtual USB transfers
---------------------------------------------

Source ``b58af7e5e2e21e86105c95193af9fb530788610c`` routes boot query failure
through the parent's existing terminal/fault-work path, closing submission
admission and joining cancellation on teardown. It adds a boot phase to the
hardware-free USB fixture without opening any real-device loader gate.
Final tested source ``d55302fd49414879a02522f5291085c18773ac13`` passes the boot
phase at both full and high speed: 14 cases, 14 exact 19-byte fixed READM OUT
transfers, 12 IN replies and 14 disconnect/drain boundaries per run.

Cases include U/USB zero and 0x0c, E/USB 0x0a, S/SDIO 0x04 and E/UART 0x28;
the latter two are observations, not transport support. Replies of 0, 1, 3,
5 and 512 bytes fail with -EMSGSIZE, rather than becoming zero-valued U
identities. A withheld reply reaches -ETIMEDOUT. Removal during the reply wait
terminates initialization; subsequent U/E instances make fresh queries. Thirteen
cases observe no extra OUT during a 100-ms window before removal. This bounded
window alone is not proof against arbitrary future retries; the driver has no
retry path. Cold-read validity still requires physical evidence.

The first full-speed attempt failed on the first quiet-window check. FunctionFS
O_NONBLOCK only bounds locking/enable waits; its queued synchronous IO still
waits for completion. The 45-second supervisor deadline killed and joined that
VM and retained FAIL. The fixture now uses an explicit non-restarting SIGALRM
handler and a periodic 100-ms timer to interrupt the read. FunctionFS dequeues
and joins its request before returning EINTR. Repeating the timer also handles
a signal arriving before the read begins. The timer is stopped after the read.
No host FunctionFS device or physical hardware was accessed.

The same final source passes the existing runtime fixture at both speeds,
17 cases each, because boot and runtime share descriptor/setup code. KASAN,
UBSAN, lockdep and DEBUG_ATOMIC_SLEEP are enabled in these VMs. The freshly
run supervisors reject 22 boot log corruptions and 32 runtime log corruptions,
plus five invalid build manifests for each phase. The single-line parent
change is strict-checkpatch clean. Earlier 69-KUnit evidence is retained at
its earlier source; this increment does not claim a new KUnit run.

Evidence manifests, including the failed attempt (SHA-256):

* ``virtual-usb-build-20261017T090921.137268Z`` (initial build PASS),
  ``e06b0ceb0fd81cf30e580c94d132a6deff204aa2bc7ae4c8a253019623d09d5a``.
* ``virtual-usb-20261017T090944.643230Z`` (boot full-speed FAIL, blocked fixture read),
  ``0a870c54ac9242005b2a5a75e1fbc714265f4f5cf48d262c7145dca6f39f5906``.
* ``virtual-usb-build-20261017T091100.982430Z`` (fixed fixture build PASS),
  ``cd8009e7cfc4b09e1d28d1efbf74fb7f4dfc1031198430f36d44c6bb98ebe7bb``.
* ``virtual-usb-20261017T091125.300681Z`` (boot full-speed PASS),
  ``ae93c747e5f0a0d16c4423a6f471835943b148045f41aaccc11eb8d47bfd427a``.
* ``virtual-usb-20261017T091145.828518Z`` (boot high-speed PASS),
  ``b5e1cd0fe7d0535eb9e8b56049fc841487f3d653654a3ef15c45858bf142a4a5``.
* ``virtual-usb-negative-20261017T091146.903761Z`` (boot supervisor PASS),
  ``c6064decce49e9d97aa91d7a9acb387ffb42706f19c2f34c442231d6eb09587a``.
* ``virtual-usb-20261017T091204.198437Z`` (runtime full-speed PASS),
  ``79a31083da70928bf7559a6c0cb27731642c42ebea0a50be5d2a2653663dcf56``.
* ``virtual-usb-20261017T091226.421499Z`` (runtime high-speed PASS),
  ``b98f81ee1b6ea6d4aac85cfbf8b677a2fc1c63d578e792ba68584e05eaa06579``.
* ``virtual-usb-negative-20261017T091227.507498Z`` (runtime supervisor PASS),
  ``c1e1852129087b4ce543d5d68ae2bc90e2250eb1f2d9b3b253964056f4f8b9f4``.

The fixed command and actual reply lengths now have synthetic USB integration
evidence in addition to injected-IO tests. This does not prove real cold U/E
register accessibility, register provenance, board/trim validity, firmware
loading, 32-KiB boot TX, READY delay, armed startup/class-control, HCI or Wi-Fi.
No new physical qualification or external submission occurred. The later
documentation commit is not the compiled/tested source identity.

Auxiliary BLE endpoint and native HCI consumer
--------------------------------------------

Source ``b440989db41e6119461cef16a601e2c8a3659e97`` adds a per-device auxiliary
BLE endpoint together with its real Bluetooth HCI consumer. The endpoint owns
serialized backend admission, atomic receive/fault callback binding and terminal
errors; backend pointers are cleared before final device release can be delayed
by external references. The consumer implements setup/open/close/flush/send,
bounded command/ACL TX and owned event/ACL RX, and deferred HCI withdrawal on
transport/parse faults. No physical USB publication call was added: qualified
firmware/identity/board/startup and a disabled/drained BLE baseline remain
required. No SLE placeholder or private userspace ABI was introduced.

Subsequent fixes clear stale auxiliary private data on failed probe/remove and
keep RX active during TX flush. Final functional source is
``3bfa3354ab86efbd7f7793d5f86a45bc06127454``. Codec exports are namespaced and
compiled once, including when its tests and the HCI driver are both built in.
The selected tristate dependencies prevent a built-in consumer with a modular
WS73 USB owner. Not all legal mixed combinations have been compiled yet.

The first regular KUnit run passes 72 cases, including three new endpoint
cases: enable/disable admission and callback gating; a failed enable retained
through removal and a delayed final reference; and invalid HCC input plus a
terminal send failure. No native HCI consumer is bound in that configuration.
A separate HCI-only configuration binds the actual consumer to an explicitly
synthetic backend that rejects enable. Native HCI registration/power-on occurs,
then the fault withdraws HCI; consumer rebind cannot clear the endpoint error.
No successful action or HCI command completion is fabricated. That configuration
passes one native lifecycle case and the existing 16 codec cases.

After the cleanup/flush fixes, the HCI-only run passes again. Test source
``84da8c9b1cc2e988b7380c8adaae5bb232f3be41`` then enables SMP, lockdep and
DEBUG_ATOMIC_SLEEP alongside KASAN/UBSAN. The HCI-only 17 cases pass under that
configuration. The separate core-only VM passes 56 cases with NET and Bluetooth
disabled under the same instrumentation. These counts describe their individual
configurations, not independent repetitions of every test in one kernel.

Two build-tool failures are retained. The initial builtin checker mistakenly
required the SPDX comment to appear in generated .config and failed before C
compilation. The first module-only build omitted the corresponding kernel and
modpost rejected missing vmlinux symbols. The corrected builder validates only
configuration requests and builds bzImage before modules; unresolved symbols
remain errors. The all-module WS73 core/USB/HCI/codec combination passes. The
repository-relative public tool passes the same combination at source
``f9298bfa4828c4caeb6a53238b7e522b6a314510`` (tools/README changes only).
Module metadata shows HCI depending on bluetooth, ws73-core and btws73_proto,
with WS73/BTWS73 namespace imports. This is compile/link evidence, not module
load/unload qualification. W=1 reports existing kernel-doc warnings outside
WS73 and an unused ACPI num_carats variable; the WS73 sources are warning-free
in this build and the new C/header files are strict-checkpatch clean.

Evidence manifests, including failed attempts (SHA-256):

* ``20261017T092630.839944Z`` (72 KUnit PASS),
  ``1232059d1c7d3533ed283c202d74cd7690f284fcf64dae87fbdb33504fd55254``.
* ``hci-build-builtin-20261017T092819.406083Z`` (FAIL, comment/config check),
  ``499520825dde18570e0425d9ecfa4d916f95b90029b56d50c9fe4f33e8c9b24d``.
* ``20261017T093024.236035Z`` (17 HCI/codec KUnit PASS),
  ``57a3fd61a4154573f9b49ab989b54dddde05ab59f143e2bba8de672f0341b4d3``.
* ``hci-build-module-20261017T093153.641025Z`` (FAIL, missing kernel symbol table),
  ``f98765ef3260bb5c9a159530750afcb20cbf4a7db6d5acdd3cdd83839b1bae70``.
* ``hci-build-module-20261017T093324.943534Z`` (module build PASS),
  ``2d72907cfad740d1468d7d28fe911b694df5c50dc86676900caf5d2f8cb2673a``.
* ``20261017T093617.967909Z`` (17 HCI/codec KUnit PASS after fixes),
  ``4ad6fafb94cabcbaff5393b6d1965057eb1d26e1c87410c2670a00b00002d1da``.
* ``20261017T093712.640719Z`` (17 HCI/codec SMP/lock KUnit PASS),
  ``e3a412747d91f07fa0597d1f472b3740c09da3cf4edb012391222304d821cf5c``.
* ``20261017T094019.100325Z`` (56 core-only SMP/lock KUnit PASS),
  ``5a7a50c58557b738cf0961127496b37d310a4c8703691d12f15b6310efe46b05``.
* ``hci-build-module-20261017T094254.259481Z`` (public module build PASS),
  ``0e337a83eccb24be545ea6804c695acf0faf30142ff9437c3c249174615dcd9d``.

The public commands and scope distinctions are in ``tools/testing/ws73/README``.
These tests do not execute successful HCI setup, command/ACL traffic, queue
saturation, active RX during flush, peer exchange or actual USB BLE enable.
Callback/backend concurrency and full module-load ordering need further tests.
The USB parent still observes identity without publishing a BLE function;
boot/firmware/board initialization integration, cold U/E evidence, peer/AP
resources, Wi-Fi, firmware delivery and upstream review remain open. No new
physical operation, RFC or formal submission occurred. Documentation commits
do not replace the exact compiled source identities above.

Native HCI setup queries, data ownership and bounded admission
------------------------------------------------------------

Final tested source ``a511afc8f138c17ec5157bb5947d4af7f2679f93`` adds an explicit
KUnit controller model attached to the real auxiliary endpoint and HCI consumer.
The model validates exact outgoing HCC commands, supplies synthetic Features and
BD_ADDR completions in fragments, then deliberately withholds Reset completion.
Both driver setup queries complete through the native command engine. This is
not full HCI initialization, real firmware behavior or peer qualification.
No response generator is included in the production driver or USB fallback.

Four new cases cover a valid ACL encoded into an exact HCC record; an empty TX
flush followed by fragmented event RX whose owned skb survives overwriting the
borrowed input; independent packet and byte limits including the in-flight
packet; and invalid ACL length with retained caller skb ownership. The blocked
Reset holds one slot: 63 further packets fit the 64-packet limit. Separately,
43 maximum-size H4 packets plus Reset fit 64 KiB and the next is rejected.
Rejected admission latches the first error, prevents queued ACL transmission,
withdraws HCI and leaves the shared endpoint terminal. Cancellation wakes the
pending native Reset request without manufacturing its completion.

The production policy follows native hci_sched_acl_pkt behavior: it charges
credits even when the driver's send callback fails. Returning a transient queue
error alone would leave accepted controller-credit accounting stranded. Active
send rejection now withdraws the instance. Flushing accepted queued packets in
an active session is also terminal; normal close gates the session first.
The byte cap is now 65536 H4 bytes, independently reachable before the packet
cap. These are software resource bounds, not device credits.

The HCI-only VM passes 21 cases: 16 existing codec cases, the earlier failed-enable
lifecycle case and four new model cases. SMP, KASAN, UBSAN, PROVE_LOCKING and
DEBUG_ATOMIC_SLEEP are enabled. The same source passes the public all-module
compile/link check with Clang and W=1. No module is loaded on the host.
Earlier regular 72/core-only 56/virtual USB results retain their earlier source
identities; they were not rerun or counted as new evidence in this increment.

Two initial compilation failures are retained: source
``e13847c34832176e1e6a436ba64f263c826f9b68`` used a bool device-match callback where
this kernel requires int; source ``f41754fea034896a281a8ac339ddede95953884c``
included unguarded hci_sync.h twice, directly and through hci_core.h. Both were
fixed before the passing VM run. Neither failed attempt executed a VM.

Evidence manifests, including failed attempts (SHA-256):

* ``20261017T095913.430674Z`` (FAIL, callback signature),
  ``13e0bacec996d3c704517c75383db44986fb0ff24e4b35baab066f5ba6d66b0c``.
* ``20261017T100213.923492Z`` (FAIL, duplicate header),
  ``820394e1447913e31c856b7f883f36369591f53901f0d4b89ccbf93354650445``.
* ``20261017T100417.059062Z`` (21 HCI/codec KUnit PASS),
  ``d06ab204f051dec44fb6430428e477717b548e74d35e4299a9f01c79ffce95b3``.
* ``hci-build-module-20261017T100727.675893Z`` (all-module compile/link PASS),
  ``c0773de9a820087a0aa3f11138f62714b1513827bf9769583ec45fe6464c5c06``.

The model calls the actual driver's send callback without establishing an L2CAP
connection; it does not execute native connection/ACL-credit scheduling. ACL RX,
queued active flush, allocation-failure injection, full HCI initialization,
BlueZ and real enable/disable remain open. The USB observer still does not
publish a BLE endpoint. Cold U/E identity, trusted board/trim inputs, parent
firmware/startup integration, BLE peer and AP resources, Wi-Fi and upstream
review remain open. No physical device operation or external submission occurred.
The later documentation commit is not a replacement tested-source identity.

Native HCI active flush, ACL receive ownership and allocation faults
------------------------------------------------------------------

Functional source ``66029e3033d6e2da258d49aeac63ebce66c41f23`` closes TX admission
under the RX-state lock on parsing/receive failure, before scheduling deferred
withdrawal. The earlier path released the lock before marking the error, allowing
an intervening TX admission. Shared fault notification still runs in the worker
and never reenters the shared callback lock.

Three added cases pass with the existing tests (24 total). ACL RX is split at
every header/payload boundary and its source overwritten; the copied skb at the
native HCI queue retains the exact type and bytes. A separate thread flushes with
Reset blocked in the backend and an ACL queued behind it. The test observes the
thread's uninterruptible wait before releasing Reset, verifies that RX remains
admitted during the wait, then checks -ECANCELED, discarded queued ACL, HCI
withdrawal and retained first error. Oversized ACL RX is followed immediately by
TX and valid RX; TX rejects with the first error and RX cannot revive the instance.
The flush wait has a bounded observation deadline and cleanup releases the
backend and joins the test thread even if an assertion fails.

Final test source ``d1b4669fba3ac17b95b13763471f138f9d4fc326`` adds two real
allocation-failure cases using FAILSLAB and the calling task's fail_nth. Injection
is cleared before KUnit assertions and the test checks that it was consumed.
The first allocation in skb_copy_expand (TX) or bt_skb_alloc (RX) fails; the
original TX skb stays caller-owned, RX publishes no packet, and both paths
withdraw HCI with terminal -ENOMEM. No production allocation hook or global
failure probability was added. This does not cover every later allocation site.

All 26 cases pass: 16 codec, one failed-enable lifecycle, nine native model
cases. SMP, KASAN, UBSAN, PROVE_LOCKING, DEBUG_ATOMIC_SLEEP, FAULT_INJECTION and
FAILSLAB are enabled. Raw guest logs contain the two expected fault-injection
traces, and the named tests pass. The public all-module build also passes at the
same final source; production modules do not require fault injection. No new
failed build or VM attempt occurred in this increment.

Evidence manifests (SHA-256):

* ``20261017T101455.054155Z`` (24 HCI/codec KUnit PASS),
  ``8f8c2428f16a31cbb36647b115643c8cd17a79f76f4eb61a5a498cd1e46724ec``.
* ``20261017T101609.844536Z`` (26 HCI/codec KUnit PASS with FAILSLAB),
  ``b3b2ac492fae615b951edf19047256ec8efceb6bcdb5a267baaeb4291518846d``.
* ``hci-build-module-20261017T101942.351421Z`` (all-module compile/link PASS),
  ``eaf6cdaef16b6afcbfd8636430b99c1dd02978b4cece079ff690ff207e08eae2``.

The model still deliberately withholds Reset: no full HCI initialization or
connection exists. HCI_INIT discards ACL after the tested receive queue, so
there is no L2CAP or peer-data qualification. This flush test is a specific
controlled interleaving, not an exhaustive concurrency proof. Actual module
load/unload, USB publication/startup integration, cold U/E, board/trim, peer/AP,
Wi-Fi and upstream gates remain open. The Wi-Fi TX-status source audit in
ws73.rst was advanced independently during this build; it provides no runtime
firmware qualification. No physical device operation or external submission
occurred. Documentation commits do not replace the exact tested source above.

Runtime BLE backend, publication ordering and USB teardown
---------------------------------------------------------

Source ``6acd0f8a27a6026000149901df9bd96e7c7a4c73`` connects the actual auxiliary
backend operations to BLE action transactions and the bounded shared USB TX
sequencer. The USB owner now routes validated queue-7 records, post-startup
action replies and shared faults through the adapter. Disconnect aborts the
adapter, joins USB/init producers, then detaches and removes the child before
freeing the parent. Physical observation still never calls publication.

Publication requires startup DONE and acknowledged BLE DISABLED; identity,
firmware/board/address validity and old-data drain remain separate owner
preconditions. Routing is installed before auxiliary_device_add can probe.
Final functional source ``c7e74a4b614a785420a533c4a5d1084b5b6c0152`` also checks
for synchronous probe faults after add and rolls back the child with the
retained error. Successful registration is not HCI or firmware readiness.

Seven new injected-I/O tests use the actual startup, customization, action,
USB TX, aggregate decoder and shared endpoint code. They cover unarmed and
still-enabled publication rejection; startup/acknowledged disable/publication,
start/send/receive/stop; delayed final references after removal; action short
transfer remaining BLE-local; TX short transfer notifying the physical owner;
synchronous probe RX before registration returns; abort inside that probe and
publication rollback; and U64_MAX deadline rejection without a control send.
A synthetic auxiliary consumer is used here, not the native HCI driver.

The first run passes 75 of 76 cases and fails the new action-short-transfer
case: its expected errno was incorrectly copied from the bulk TX contract.
The existing action engine returns -EMSGSIZE; the test expected -EREMOTEIO.
The assertion was corrected to the existing contract, without weakening checks
or changing production error behavior. This failed attempt remains recorded.
The final regular configuration passes 79 cases (existing 72 plus seven link
cases). A separate native HCI/codec VM passes its 26 existing cases after the
auxiliary allocation/registration refactor. These are separate configurations,
not an end-to-end native HCI-through-USB qualification.

At the same final source, the hardware-free USB build passes with NET disabled.
Runtime full/high-speed fixtures pass 17 cases each; boot full/high-speed pass
14 each. They exercise observation and teardown with no published BLE child,
not class-control/startup or BLE traffic. The fresh negative supervisors reject
32 runtime and 22 boot log corruptions, plus five artifact corruptions per phase.
The required artifact sets now include ble.o and ble-link.o. The public all-module
build passes too, including the new WS73 namespace exports. No host module was
loaded. The changed C/header files are strict-checkpatch clean.

Evidence manifests, including the failed attempt (SHA-256):

* ``20261017T102843.804386Z`` (FAIL, action errno assertion),
  ``9d315135116fff1228f495f20c5ef1e5001ab422a25012bef26dd1d5d75d94c2``.
* ``20261017T103240.003553Z`` (79 KUnit PASS),
  ``3d1ca498c96cac06d74c8922c58c0ed86b8d67db87227163e52315d120424262``.
* ``20261017T103316.478857Z`` (26 HCI/codec KUnit PASS),
  ``db9bb43523d1bc4a57b3734f93b4a7488271fbb5ce0ff0279cf17b8bf6ccdfd5``.
* ``virtual-usb-build-20261017T103631.848315Z`` (USB build PASS, NET disabled),
  ``55010ea7f64a87c5b02cda69204f3b049251af90249060bfef75abd367d734a4``.
* ``virtual-usb-20261017T103707.855174Z`` (runtime full-speed PASS),
  ``b95656468473626a864ea6fb5354ac08cb51ea4ef44fd0f2a24ca7590b64d8e7``.
* ``virtual-usb-20261017T103718.516891Z`` (runtime high-speed PASS),
  ``14a0824b4fc53ec14d54c8e03733ccc951e97e5621fa150b3c01b11c72cd02a8``.
* ``virtual-usb-20261017T103728.605798Z`` (boot full-speed PASS),
  ``b2de04702e7cf2e4316637134503714740c13a0910d58eb7db1805a73ada7dbf``.
* ``virtual-usb-20261017T103740.537886Z`` (boot high-speed PASS),
  ``86324ca057243752c17724c5a0c9dcfafc42aec9c9303cdc821b791bcc5318e6``.
* ``virtual-usb-negative-20261017T103810.547135Z`` (runtime supervisor PASS),
  ``2ca7c5123edd5d2d5d1f880cbe44739f5543bba37faf96ccb547d206783ff5cf``.
* ``virtual-usb-negative-20261017T103810.543217Z`` (boot supervisor PASS),
  ``f75cba7fbcec07692ba59d53e74eb2b52e458ecf1742f209554d879ee1ee11c4``.
* ``hci-build-module-20261017T103800.415851Z`` (all-module compile/link PASS),
  ``4fbb7504de292b4c49bcdd9bb776c044d5bdc6d949d3251c329774a79e0e3578``.

The KUnit/USB VMs retain SMP, KASAN, UBSAN, lockdep and DEBUG_ATOMIC_SLEEP;
HCI-only also retains FAILSLAB. No physical operation, firmware download or
external submission occurred. Cold U/E, trusted board/trim and epoch, hardware
data drain, parent loader/startup integration and publication, real USB class
requests, full HCI initialization, BLE peer/AP, Wi-Fi and upstream review remain
open. Documentation-only commits do not replace the tested source identities.

Native HCI composed with the runtime BLE backend
-----------------------------------------------

Source ``27358a7b9dacf800cae013365d78ab245fa7bceb`` adds an explicit controller
model path through the actual startup, runtime BLE adapter, USB TX sequencer,
auxiliary endpoint and native HCI consumer. It retains the earlier direct-backend
cases for comparison. Synthetic BSP/customization and action acknowledgments
complete startup, followed by acknowledged disable and publication. Native open
then sends the actual four-byte enable command through the action engine.

The model independently checks USB sequence counters, queue switch/reset order,
reserved prefix bytes, total sizes, HCC command/ACL bytes and padding. Queue-10
notifications and fragmented HCI replies are wrapped as synthetic USB aggregates
and decoded before routing. Actual native Features/BD_ADDR queries complete;
Reset is transmitted but deliberately not answered. The data case sends ACL
through the driver worker and USB framing, then checks a coalesced event/ACL
stream split across two HCC records. Both owned skbs retain exact bytes after
all source buffers are overwritten. HCI_INIT still drops ACL before L2CAP.

A short enable transfer is rejected despite the model supplying an action reply;
HCI withdraws without issuing any HCI command or faulting shared TX. A short
bulk data completion faults the shared transport and withdraws HCI. The model
saw that ACL, but the driver records local TX failure rather than credit or ACK.
The initial composed configuration passes 29 cases.

Final source ``3a5c44d5a17840bc3f8cdc0181d2f9938d165d55`` adds parent abort while
Reset is blocked inside the runtime TX backend and an accepted ACL is queued
behind it. Abort closes admission before the model releases the blocked call;
HCI unregister joins its work, no queued ACL reaches the backend, RX cannot
revive the instance and a held auxiliary reference returns -ENODEV after removal.
The final run passes 30 cases: 16 codec, one failed-enable lifecycle, nine prior
native model cases and four composed model cases. SMP, KASAN, UBSAN,
PROVE_LOCKING, DEBUG_ATOMIC_SLEEP and FAILSLAB are verified in both configurations.

Evidence manifests (SHA-256):

* ``20261017T104605.823705Z`` (29 HCI/codec KUnit PASS),
  ``0b4863d87f1c51c08ed2c1f6c235553028f7bd5af5292ecf32ba0191b9cb61fe``.
* ``20261017T104801.909198Z`` (30 HCI/codec KUnit PASS),
  ``c0aedd04f510669846e45586dfcb7c12968676ede91db1ecf2a9baa4d1d53af8``.

This increment changes only tests and their expected-case supervisor; production
code is unchanged. No failed test attempt occurred. Earlier regular 79-case,
USB fixture, supervisor-negative and module results retain their earlier source
identities and are not claimed as fresh runs. The changed test file is
strict-checkpatch clean. This composition reaches the injected-I/O boundary,
not an actual USB control setup packet, submitted URB or physical firmware.
No full HCI initialization, L2CAP/peer, cold U/E, board/trim, hardware drain,
parent firmware-loader integration, Wi-Fi or upstream qualification is added.
No physical operation or external submission occurred. Documentation commits
do not replace the exact tested source identities above.

Synthetic boot download through actual USB requests
---------------------------------------------------

Pure extraction ``ef9b89ce921e5f8d945a59368652991cfc1cc9ca`` moves the existing
bounded synchronous URB and READY-wait implementation to private usb-io helpers.
The physical observer and a separate dummy_hcd-only download test share them.
No physical loading or publication switch is added. The test uses ffff:3734,
requires the dummy_hcd controller and is disabled in the native HCI module
build. The ordinary physical observer retains ffff:3733 and its existing gates.

Initial source ``245fd9745f264c243f61aef42064e9d6de34fc19`` builds and passes
seven full-speed download cases. Source
``78f0a4534b581dd30931158fe7d05a5d7f983dae`` adds direct timing of the actual
shared sleep, because gadget wire timing includes USB scheduling and cannot
alone prove the minimum delay. Its full-speed run passes; its high-speed run
fails on the first payload: the fixture supplied a 64-byte buffer for a
512-byte USB packet. FunctionFS reported overflow and production download
reported -EREMOTEIO at PAYLOAD. The failed attempt remains in the evidence.

Final tested source ``28fd7e8a947c22a8e9646066233d0e6d357cd9d6`` reads the
negotiated endpoint descriptor and uses the actual 64/512-byte first-packet
size. No production transfer rule was weakened. Both speeds now pass all
seven cases: success, malformed READY, malformed FILES OK, withheld READY
(timeout), removal at READY, removal at payload admission and fresh success.
Each success independently checks four FILES commands, all 32769/32/32 bytes
and exact QUIT. Failures cannot advance progress or send QUIT. Five cases
check a 100-ms no-extra-OUT window; all seven require final callback drain.
The payload-removal test synchronizes on admission, not a claim that every
scheduler interleaving cancelled an already-submitted OUT URB.

The two final download runs each check ten shared sleeps, all at least 5 ms.
Measured ranges are 7894332..8625262 ns (full) and 7987025..8344305 ns (high).
These are VM scheduling observations, not performance targets. Exact transfer
and reply lengths remain checked by production code. Successful download
reports stage WAIT_RUNTIME, image 2, acknowledged 32; it never reports Ready.
No real firmware file, hash/profile selection, RAM/XO preparation, cold identity
or runtime re-enumeration is exercised by this explicit synthetic payload model.

On the same final source, existing runtime 17-case and boot 14-case USB suites
pass at both speeds. Download/runtime/boot supervisors reject 28/32/22 log
corruptions and five invalid build manifests each. The artifact set now includes
usb-io.o and usb-boot-test.o. The all-module HCI/core/USB compile/link passes,
including ws73-usb-io.ko and namespace dependencies; the test-only driver is
disabled there. No host modules are loaded. USB VM configurations verify SMP,
KASAN, UBSAN, PROVE_LOCKING and DEBUG_ATOMIC_SLEEP with NET/KUNIT disabled.

All attempts and their manifest SHA-256 values:

* ``virtual-usb-build-20261017T110133.928661Z`` (PASS),
  ``5c0e83366407124457a05b77587d41388e1a5d21319152977dbe894ac2e9ab48``.
* ``virtual-usb-20261017T110200.842070Z`` (download full PASS),
  ``7611490ae0a3cbde937617ceeb8cf00e84cd276832589e7812414b577f43b415``.
* ``virtual-usb-build-20261017T110247.847233Z`` (PASS),
  ``f5dd2d4c31b7192984d3577c8a36409e5085e885edb0b0dfbea45b5737c6fdcd``.
* ``virtual-usb-20261017T110309.401309Z`` (download full PASS),
  ``bb83248bddc35d07d22dbba2b45acc88434d6a3442cd67a466cd4481a9c9f12b``.
* ``virtual-usb-20261017T110317.555374Z`` (download high FAIL),
  ``4e1e7788830eb68d0a1dd7dff51caf8c3ea9cba590f944d8421a2346eb2d5cc4``.
* ``virtual-usb-build-20261017T110503.505695Z`` (PASS),
  ``a9820c7b2197a14a8f50b0208bc81123abb1b1f0b03f30eedf771c61c1146a2a``.
* ``virtual-usb-20261017T110527.915325Z`` (download full PASS),
  ``9b8baf3ab4edfab4e1bfda3cbece6e28d5d3d9dc3ba8b17bb5cefe768d786509``.
* ``virtual-usb-20261017T110535.876238Z`` (download high PASS),
  ``e488fa88ed2e7bd7076b1a423ac0d3b3f5cd64374bdddaa8f6006e35e6e43f64``.
* ``virtual-usb-20261017T110543.579620Z`` (runtime full PASS),
  ``19ee29de07ec58fad069f6958dd91b395b9e591269a49240f377cae6d28c8d6b``.
* ``virtual-usb-20261017T110553.856556Z`` (runtime high PASS),
  ``808d53821cb749d9f93dba1ce73f2d76cdef2870555f0059a7d8524b940b30cb``.
* ``virtual-usb-20261017T110603.987480Z`` (boot full PASS),
  ``8c9eb01b4c9ce9f9a1ff1d3edf6882c3b1af50cc7ebadd77416ed9d842642035``.
* ``virtual-usb-20261017T110615.658179Z`` (boot high PASS),
  ``d1249f659eecf61875b7cae9670bfa955bb7006fc9444072b60661002126f6d6``.
* ``virtual-usb-negative-20261017T110638.524279Z`` (download  PASS),
  ``6b2283a1a9426e233e10b822dc27f312711f09e525a6fe41887eeceb77418317``.
* ``virtual-usb-negative-20261017T110638.530507Z`` (runtime  PASS),
  ``e84350182c5e57a3a3cf6f1c979d58b4fedb4522239df924c7aaa0028b1a40e0``.
* ``virtual-usb-negative-20261017T110638.523658Z`` (boot  PASS),
  ``46cbe36ebebd691ac2f62d25a376cf6ade5b2af0f76b3fa50af6eab16ba545d1``.
* ``hci-build-module-20261017T110640.322019Z`` (PASS),
  ``8597c8d1ebb84e032d02388cf3d3e1d77646737cfb4da769a364bb82f532dd53``.

Unchanged codec, HCI-model and abstract loader tests retain their earlier source
identities and were not rerun as new qualification. The synthetic USB driver
is strict-checkpatch clean; the three USB builds have no compiler warnings.
Physical cold U/E, board/trim, parent loading/startup, actual BLE control USB,
full HCI/peer/AP, Wi-Fi, PM/recovery and upstream review remain open. No physical
operation or external submission occurred. Documentation commits do not replace
the exact compiled/tested source identities above.

Original-file acquisition joined to boot identity and transfer
-------------------------------------------------------------

Tested source ``441473b5835e7d6cce733dc5673bb1801bc23fba`` adds the one-shot
``ws73_boot_start`` entry. It copies/range-checks explicit trim, reads the fixed
HCC identity, selects exactly one U/E USB profile, requests/validates all three
original files, then runs fixed preparation and FILES/QUIT. Firmware references
remain owned until synchronous IO has joined and are released on every return
path. File acquisition now uses request_firmware_direct without a userspace
fallback. A single absolute deadline is retained; cancellation/time are checked
after file acquisition before writing. The call is serialized by its owner,
retains the first failure and rejects reuse. Success is WAIT_RUNTIME only.

The additional dummy_hcd-only ID ffff:3735 exercises this entry with original
local SDK110 files in a private VM initramfs. Both speeds pass ten cases:
U and E success; missing U BTC; changed U code with a recomputed valid internal
hash; E Wi-Fi calibration in the U path; S/SDIO; E/UART; identity-stage removal;
malformed first WRITEM acknowledgement; and fresh U success. Each calls start
again and verifies the same first error or -EALREADY, with no repeated IO.

The peer verifies exact READM and four-byte replies, all 11 preparation writes
with explicit synthetic trim 1/2, FILES flags/addresses, every original payload
byte after the 64-byte header, and QUIT. The three successes per speed transfer
22 chunks in total, including two E BTC chunks. Missing/corrupted/mixed inputs
and unsupported transports stop before the first write. Bad WRITEM stops before
FILES. Every instance drains, and nine quiet windows check no extra OUT.
Identity removal returns -ENODEV at full speed and -ESHUTDOWN at high speed;
both retain the error and do not publish an identity or input state.

The loader supervisor rejects 33 log corruptions and nine artifact/provenance
corruptions. It also rechecks all six original file identities and the exact
rehashed negative input. The runner preserves a FAIL manifest for preparation
errors as well as VM errors. An intentionally empty local firmware directory
is rejected before QEMU starts; that expected FAIL attempt is retained along
with its successful negative-test receipt. No unexpected failed attempt occurred.

Regular KUnit passes 80 cases, adding one loader guard case with eight scenarios:
coarse/fine range, missing device/callback, expired deadline, cancellation,
retained failure and completed session. Repaired arguments cannot revive them
or issue IO. Existing synthetic download full-speed seven-case regression and
all-module compile/link also pass. SMP/KASAN/UBSAN/PROVE_LOCKING and
DEBUG_ATOMIC_SLEEP configurations are verified; the module build disables the
synthetic USB test driver. Strict checkpatch passes for the new loader and test.

Evidence manifests and SHA-256 values (all bind to the tested source above):

* ``virtual-usb-build-20261017T111750.615422Z`` (PASS),
  ``f8611074b3be6f576df908a8b214bbea927351aacc97256d8393c625658caa7a``.
* ``virtual-usb-20261017T111837.757133Z`` (loader full PASS),
  ``2c3341d510c00fbdee85ea1db43a219d54bfc3fdee4f65115fd3cac709545f0a``.
* ``virtual-usb-20261017T111926.614383Z`` (loader high PASS),
  ``0d1f5742be026652d1829b468da1806ec8d91d6bc8eadb568a8739cf513ecc81``.
* ``virtual-usb-20261017T111934.843785Z`` (download full PASS),
  ``f51a074526f80af81415ead80c2c46f3c4066a866ac017fb730503b065240a84``.
* ``virtual-usb-negative-20261017T111924.312351Z`` (loader  PASS),
  ``c655cfe4be87f576a58047344f1ddf82eb7ed92a1bebe2d44874fdc40d5c01b1``.
* ``20261017T111942.917462Z`` (PASS),
  ``16f22d11ec5db971597350f3557762595a5e31a2ee25d70292b65f9de96f68d0``.
* ``hci-build-module-20261017T112237.822943Z`` (PASS),
  ``aff3b78c40c515390145db418743ba9301fd44778d632a0fa287e4752ee15248``.
* ``virtual-usb-20261017T112327.866566Z`` (loader full FAIL; expected missing-local-input rejection),
  ``538548e9a63f0e3306ff2e6288663c18e4b0a90d6e44c90c00973656d2d02944``.
* ``loader-runner-negative-20261017T112327.634380Z`` (PASS),
  ``333eb6b509862b201eaf7f16763706d4ecdea1662704dd2c2a1ecb75f349fb75``.

The original binaries and generated negative file are local artifacts only;
only hashes and source are committed. This does not establish firmware
redistribution permission. The physical USB observer does not call the new
entry: cold U/E access, board/register/trim applicability, runtime re-enumeration
and identity/version agreement remain open. No physical firmware execution,
actual BLE class-control/startup USB, complete HCI/peer/AP, Wi-Fi or upstream
qualification is added. File acquisition remains synchronous and was not
subjected to deterministic cancellation/latency injection here. No host module
load, physical device operation or external submission occurred. Unchanged
runtime/boot USB and native HCI-model results keep their earlier source identities.
Documentation/help-only changes do not replace the exact tested source above.

Current shared USB IO on the assigned physical U
------------------------------------------------

Clean compiled source **517bc7a13ebda6225f3b0bf329ed7988bebf6e20** was tested
on this project's assigned physical WS73U at ``1-1.1.1``. This requalifies the
boot observation after the shared USB IO extraction and current owner changes;
it is separate from dummy_hcd/FunctionFS results. The fixed observation build
uses LLVM/W=1, four jobs, SMP, KASAN, UBSAN, PROVE_LOCKING,
DEBUG_ATOMIC_SLEEP, DYNAMIC_DEBUG and usbmon. NET, KUnit and USB gadget support
are disabled. The private KVM has two vCPUs and 1 GiB, with no disk or NIC.
The previously recorded initramfs observer is unchanged; the kernel and all
WS73 build objects are newly bound to the source above and archived with hashes.

Before each attachment, physical topology, parent hub, exact descriptors,
serial, VID/PID, driver absence and usbfs ownership were checked. Only the
allocated device was passed through. The successful attempt records:

* OUT endpoint 1: exactly 19 bytes ``READM 0x40019000 4 ``, successful completion.
* IN endpoint 0x81: capacity 512, successful completion of exactly four bytes
  ``00000000``; production decoding reports package 0, bus 0 (U/USB).
* QMP device removal, guest disconnect and exactly one production
  ``device callbacks drained`` record, followed by QEMU exit 0.
* Post-exit device identity, descriptors and topology match; no host driver
  or usbfs owner remains. No KASAN, UBSAN, lockdep, atomic-sleep or kernel
  warning is present in the captured log.

Result: **OBSERVATION_PASS_NOT_COLD**. The device was not power-cycled.
These transactions exercise the physical USB controller and current driver;
they do not qualify cold register accessibility, firmware loading, runtime
identity agreement, BLE/HCI communication or Wi-Fi. No WRITEM, FILES, QUIT,
calibration data, firmware payload or HCI command was sent. QMP removal is
an injected disconnect, not proof of natural recovery or a power cycle.

All attempts remain recorded. The first build failed because ccache attempted
to write outside the sandbox; compiler feature detection also lost KASAN.
The next build explicitly disabled ccache, restored required configuration,
and verified it. A no-device VM boot then passed. The first hardware attempt
completed READM and removed the device, but the runner failed waiting for the
debug-level drain message: default console level 7 excluded it. The next
attempt changed only the VM command line to ``loglevel=8``, captured the
message and passed. Neither failure is overwritten or called a complete pass.
The trace verifier accepted two fixtures and rejected all 13 negative controls;
this tests the verifier only and is not a hardware qualification count.

Local records under ``.dev/ws73-upstream/records`` and manifest SHA-256:

* ``observe-build-20261017T113457.405789Z`` (FAIL),
  ``8cbec0f88bcf578a8ab120bbfe35231f477182ef8eaac5e143265d9991407fdb``.
* ``observe-build-20261017T113551.523302Z`` (PASS),
  ``bc3cac0e6de71fb7ab5d656ae8209bb9864be72d4f83d0409db47b57dcdff3a5``.
* ``usb-observe-20261017T113900.231488Z`` (VM_BOOT_PASS_NO_DEVICE),
  ``08b7cfbce137fe9a3560beabd9aa2bacf5f72509e50b766df172990a65963eb4``.
* ``usb-observe-20261017T113914.952426Z`` (FAIL),
  ``8b6e1bf06db7922563c18241e5c0381376adc883a228929f8e1d44a68389d8b7``.
* ``usb-observe-20261017T114006.401592Z`` (OBSERVATION_PASS_NOT_COLD),
  ``d77efcd9efcf31b27c6b2edc57f8d0af1878a3ff5dbdb46d9ff050ae6c66fdbd``.

The final trace-verifier receipt is ``observer-trace-negative-current-v3.json``,
SHA-256 ``b59db61b0db126867e2d467169777bc66d80bc67b4c0e3b1a8081e9df7b4c31e``.
Runner, initramfs, build proof, configuration, tool identities, kernel/objects,
QMP exchange, all raw USB records and kernel/QEMU output are retained locally.
Unchanged codec, synthetic USB and native HCI-model tests retain their previous
source qualifications; they were not rerun for this documentation update.
The next physical gate is a confirmed power-cycle observation, followed by
qualified board/trim inputs and this project's own firmware/runtime path.

First U observation after user-confirmed power removal
----------------------------------------------------

After the non-cold test above had released the device and exited its VM, the
user confirmed physically unplugging/replugging only the assigned WS73U and
that it had no other power supply. This statement is the power-history evidence;
no software reset or QMP detach is substituted for it. A fresh topology and
ownership check found the same port, serial, VID/PID and exact descriptors,
with host USB address changed from 105 to 116. The new private VM used the
same compiled source **517bc7a13ebda6225f3b0bf329ed7988bebf6e20**. Worktree HEAD
was ``64152b4cd99a944ae263b455e64ac4c73c2f1b09``; the runner verified that its
only differences from the compiled source were two documentation files.

Before any calibration/register write or firmware transfer, the production
USB parent sent exactly the 19-byte fixed READM. Its OUT completed successfully;
the 512-byte-capacity IN completed successfully with exactly four bytes
``00000000``. The driver decoded U/USB. This supports the observed zero value
being a valid returned value on this U sample after power removal, rather than
merely a value observed following previously loaded firmware. It does not prove
all inaccessible-register/failure behaviors or the mapping for other hardware.

QMP removal again produced disconnect and one drain record, QEMU exited 0, and
post-exit checks found the same device identity with no driver/usbfs owner.
No sanitizer, lock or kernel warning was found. Result:
**COLD_OBSERVATION_PASS_USER_CONFIRMED_SINGLE_U**. This is one cold observation
on one U sample, not the planned repeated U/E qualification. No firmware,
WRITEM, FILES, QUIT, BLE or Wi-Fi transaction was issued. The E device, repeated
cold starts, board/trim applicability, firmware/runtime identity comparison
and native function communication remain open gates.

Local record: ``records/usb-observe-20261017T114201.435882Z``; manifest SHA-256
``23556a4c23445fd043278b97b6d3a1894a3749153452359fd4a49a88748e9565``.
The final runner trace-verifier receipt is ``observer-trace-negative-current-v4.json``,
SHA-256 ``b1afd34c9bed6e1127d7cd069b4baa0acc6de49fb848b19b736cfdc19936b4a2`` (two accepted
fixtures, 13 negative controls rejected; verifier scope only).

Boot VERSION with actual U evidence
----------------------------------

Clean compiled source **6c9cffe6cac65dfbc15e52c8200698bdc5714b20** adds the
fixed SDK110 VERSION command after successful boot HCC identity, sharing one
four-second absolute deadline. Replies are opaque actual bytes (1..64), with
no implied NUL terminator, version acceptance table or firmware compatibility.
The SDK helper and command dispatcher exist; the inspected default download
table itself does not schedule VERSION. Trim remains an explicit board input;
SDK INI ``8,60`` was not installed as a universal default.

Review before execution corrected the receive capacity to the SDK's 64 bytes.
A full-speed 64-byte reply without an extra zero packet need not terminate a
512-byte read. The final implementation keeps the actual transfer boundary;
it rejects counts outside the buffer contract but does not claim it can detect
arbitrarily queued extra packets. Physical firmware loading remains unopened.

All **81 KUnit cases** pass, including response counts 0..513, non-terminated
64-byte data, failure output preservation, short TX, errors, cancellation and
deadline checks. The actual USB boot fixture passes **14 cases at each of full
and high speed**, with seven VERSION exchanges per speed. Two per speed send
64 bytes and five send three bytes; the FunctionFS fixture does not enable
zero packets. Failed identity sends no VERSION. Both speeds retain 14 drains
and 13 no-extra-OUT checks. The supervisor rejects **27 log** and **five artifact**
negative controls. The all-module core/USB/BLE build links successfully.
SMP, KASAN, UBSAN, lockdep and DEBUG_ATOMIC_SLEEP remain enabled and checked.
No unchanged native-HCI model or unrelated USB phase is claimed as rerun.

The allocated physical U at ``1-1.1.1`` was independently checked for topology,
serial, exact descriptors, ownership and access, then attached only to this
project's private two-vCPU, 1-GiB KVM. Its actual traffic was:

* READM OUT 19 bytes completed successfully; IN capacity 512 returned exactly
  four bytes ``00000000``, decoded as U/USB.
* VERSION OUT eight bytes completed successfully; IN capacity 64 returned
  exactly 12 bytes ``424f4f545645523030310000``: ``BOOTVER001`` plus two NULs.
* QMP removal was followed by guest disconnect and one production callback
  drain message. QEMU exited 0; identity/descriptors still matched and no
  host driver or usbfs owner remained. No sanitizer or kernel warning appeared.

This attempt is **OBSERVATION_PASS_NOT_COLD**: no fresh power cycle was requested
or counted. The earlier user-confirmed cold U observation keeps its own source
and evidence identity. No WRITEM, FILES, QUIT, original firmware payload or
HCI command was sent. The ROM string does not prove firmware execution,
version compatibility, BLE/Wi-Fi functionality, E behavior or natural recovery.

The first attempt with this kernel failed before device_add: kernel printk
interleaved with the recorder's READY marker on the same serial console.
Its log, timeout and exit 0 remain recorded. The corrected recorder uses
separate serial outputs for kernel messages and usbmon/READY, with sources,
initramfs, command and hashes retained. This removes the observed mixed-output
failure without weakening the transcript checks. A separate verifier accepts
two fixtures (including full 64-byte data) and rejects 20 negative controls.
usbmon text records at most 32 data bytes; the verifier checks all available
bytes plus the full length-bounded driver observation, rather than fabricating
uncaptured bytes. The physical reply here is only 12 bytes, all captured.

The resource wait before building was verified against the active external
12-job build. No duplicate test was launched; once it ended, the pending
four-job KUnit run proceeded. All subsequent builds used fixed project output
directories and four jobs, without loading any host module.

Local records and manifest SHA-256:

* ``20261017T120858.101745Z`` (PASS),
  ``afaa94d71866454390f141792c56d21cb174284caa4f83d795237ec0fcc509f5``.
* ``observe-build-20261017T120949.552883Z`` (PASS),
  ``16e1854f09282f16da5b8be0280568d9e34c44a1ac6d35b52910af35ddde69c3``.
* ``usb-observe-20261017T121034.683222Z`` (FAIL),
  ``f9c83e4014d74742bb40a3a5cc07de548729619d32855c6aa7dec2accaf7bbc4``.
* ``usb-observe-20261017T121229.870719Z`` (OBSERVATION_PASS_NOT_COLD),
  ``05a4bd55e87c839b6f3fda9041d8ed5d786d3e28ca5f0bd66bc8554a6d3a14e1``.
* ``virtual-usb-build-20261017T121251.608052Z`` (PASS),
  ``b00623e5f274428a48c28b3ad131ac205734bdb3425cf5e1f47a85c6e78f1f05``.
* ``virtual-usb-20261017T121320.104147Z`` (PASS),
  ``bcd545e73b0aa066450d04e26db5f6e3d3e780346f5028df04541ed964458b06``.
* ``virtual-usb-20261017T121343.322038Z`` (PASS),
  ``acf9a8cb6a0bd87bcff6c1b1737d8e10718f39c1b717259e7669733d217929e6``.
* ``virtual-usb-negative-20261017T121344.393697Z`` (PASS),
  ``0f5e243f784fd7bf168a7d5121d9495ab3d2df4d3534395f99d3e1d6da54eb86``.
* ``hci-build-module-20261017T121355.497867Z`` (PASS),
  ``20bd18fd70da37461340e71d9052f170957d642dd41c8b824dc95bb1dd2bb90e``.

Final private trace-verifier receipt: ``observer-boot-info-negative-v4.json``,
SHA-256 ``02f332029c3e01cfda88c23b03344e9e26d4198e6fc310c42f80fe94d9ab60ed``.
SDK query/trim provenance: ``boot-version-provenance.json``, SHA-256
``77e73034a01672ad92ec632a921ba77a14ba2da2dcf4522da2c1a0d2ed2caf87``.
The board-model/INI source question remains unanswered. Generated CLDO
register definitions and board/trim applicability still need resolution before
physical loading; the loader/startup/native-HCI work must continue toward real
function communication. These are open dependencies, not substituted passes.

Shared USB owner, runtime startup and physical regression
---------------------------------------------------------

Production-owner extraction was signed separately as ``3599aa5ea7ec``.
The new startup integration was first built at
``2906946c192b070460481e2ff8585cc7f5ba8b62``. Its seven fixture cases completed,
but the supervisor rejected the record: it expected debug-level drain messages
in the serial console although the fixture had captured them from /dev/kmsg.
The FAIL record is retained. The checker was corrected to require the actual
captured kmsg drain, its ordering and uniqueness; info-level result messages
remain cross-checked between kernel and fixture logs. This does not turn the
old record into PASS.

The corrected clean source
``8ba42eed34b2cf2d3943ea49d20bc79b1197e2f1`` passed:

* Startup USB: seven cases at each speed, including actual control requests,
  independent action replies, STALL, timeout, malformed reply, removal, panic
  priority and a fresh success. Per speed: 46 bulk OUT, 30 bulk IN, seven
  interrupt transfers, seven class SETUPs, six class payloads, seven drains
  and six bounded no-extra-OUT checks. These are synthetic runtime devices.
* Existing runtime USB: 17 cases at each speed. Existing boot USB: 14 cases
  at each speed. Both use the refactored production owner.
* Startup, runtime and boot supervisors: respectively 26, 32 and 27 log
  rejection controls, each with five build-artifact rejection controls.

``97b978a49a0bdf754da06b6fd03705d1acf4de73`` then removes only three blank lines
before exported-symbol declarations; a diff ignoring blank lines is empty.
That exact clean source was separately compiled for physical observation and
for the all-module build. The latter includes the new startup consumer and
its namespace dependency on ws73-usb. No module was loaded on the host.
KASAN, UBSAN, SMP, lockdep and DEBUG_ATOMIC_SLEEP remain enabled in the tested
VM configurations. Checkpatch has zero errors and one generic new-file
MAINTAINERS reminder; submission/maintainer routing is still open. The earlier
81 KUnit and native-HCI model results retain their own source IDs and are not
claimed as rerun by this USB-only increment.

The assigned physical U was checked at 1-1.1.1, bus 1/address 116, with its
expected serial, ancestry, exact boot descriptors, no host driver and no
usbfs owner. A private two-vCPU, 1-GiB VM received only this device. READM OUT
19 completed, and IN capacity 512 returned exactly ``00000000`` (four bytes).
VERSION OUT eight completed, and IN capacity 64 returned exactly
``424f4f545645523030310000`` (12 bytes). QMP removal produced one disconnect
and callback drain; the VM exited 0, and the device remained unowned with
unchanged identity/descriptors. No kernel warning or sanitizer report occurred.
This is **OBSERVATION_PASS_NOT_COLD**; no new physical power cycle occurred.
The earlier user-confirmed cold U result remains separate. No WRITEM, FILES,
QUIT, firmware payload or HCI command was sent to the physical device.

Physical startup/native-HCI communication is still unqualified. The fixture's
parameters and replies do not establish board/trim applicability, firmware
epoch or U/E compatibility. No E, BLE peer, AP, natural recovery or RF support
is added. Issues remain open and no external submission was sent.

Local records and manifest SHA-256 (all attempts retained):

* ``virtual-usb-build-20261017T122551.127876Z`` (PASS),
  ``940173477b896cf385d819c7ba846de19c7f4c7b528fac0c6303eb5fd7cf8b72``.
* ``virtual-usb-20261017T122627.391283Z`` (FAIL),
  ``72477f023dcdd28dde71535ed5360d0d80c06526cb3c30ee653b8ec2f5b0b0cb``.
* ``virtual-usb-build-20261017T122704.227678Z`` (PASS),
  ``231ac293c18d3568d02b89139cb915a6c54b139587e2f0d63f41bed741305bac``.
* ``virtual-usb-20261017T122742.325585Z`` (PASS),
  ``c1a20b204dbd7e7da72730d8d682b397beb1b8b3ebb6fad418f6cb63625d0c58``.
* ``virtual-usb-20261017T122750.392961Z`` (PASS),
  ``c7e3cdd154821bb4f0ee7a041f51143cf4990d133540928e301dfae3aaa40eea``.
* ``virtual-usb-20261017T122758.455610Z`` (PASS),
  ``a340cceebe2b2353356ec854732d4b27e77ba9a8c737db63064ed9c5980c7667``.
* ``virtual-usb-20261017T122808.777964Z`` (PASS),
  ``56149a962cd8adeb0e8bdbfb2737fe80d974f7365e6e7bb1ebad79887f264ffb``.
* ``virtual-usb-20261017T122818.947273Z`` (PASS),
  ``7a4dfc2b6677930fa194fa08adf762d40481bc0e1b507f762e272662948a03de``.
* ``virtual-usb-20261017T122830.685497Z`` (PASS),
  ``62e953ea62655a94cedcf0e4f914521bfd5c921f0eb2fcf36c91d3f954aedf8f``.
* ``virtual-usb-negative-20261017T122828.037847Z`` (PASS),
  ``31304a049aa66e8e4fbdc7db715ecebbd9886ce9b412fd3c12085d195788998c``.
* ``virtual-usb-negative-20261017T122849.556250Z`` (PASS),
  ``06f318dd0e77dac59403305f0efe8a79e8193b1eb91c9005334636e38a178393``.
* ``virtual-usb-negative-20261017T122849.655680Z`` (PASS),
  ``aaf16619dc42f731123d9408d2ed9c3406ec11bcb4474257a6a4571bb309c8f2``.
* ``observe-build-20261017T122901.742470Z`` (PASS),
  ``1d7f9c439de2c74961da7c183be0d6b6109c666913e8dec65111b5cfbef41a36``.
* ``usb-observe-20261017T122942.101027Z`` (OBSERVATION_PASS_NOT_COLD),
  ``eda9ce300beac0de3e76ffc1a8c0c95c63b617a033782310788f5d559cff4851``.
* ``hci-build-module-20261017T123008.861596Z`` (PASS),
  ``eeed7d7f27cf303321e3e1503fb32c8f078944369cb81ed7b4c8af1c3f150bfc``.
* ``usb-startup-static-20261017`` (CHECKED),
  ``bedb5b0d43e163c5c50956532cae5cd902c64af1bad5a1fdca05f76f6853e50b``.

Native HCI initialization over actual virtual USB transfers
----------------------------------------------------------

Clean tested source: ``8877b3bcf78b480f76b608a40c68b5b784300f8a``.
The shared startup test consumer gained a separate dummy_hcd-only identity
that publishes the real auxiliary endpoint after acknowledged initialization
and disable. The production native HCI adapter then executes setup and full
initialization over the shared USB owner. All controller replies live in the
explicit userspace fixture. Its minimal LE-only features and absent optional
command bits are test inputs, not discovered WS73 capabilities or a Bluetooth
conformance model. No physical driver bypass or generic default address is
introduced.

At each of full/high speed, three cases pass: native initialization and
acknowledged auto-off, cancellation by USB removal while Reset has no reply,
and another full initialization on a fresh instance. Each successful case
requires a standard management configured-index event and a 289-byte Read
Controller Information response with matching fixture address/version/
manufacturer and LE-only settings. Native auto-off executes real class OUT
message 26 plus independent q10 acknowledgement. The pending command is
cancelled without a fabricated completion. Disconnect drains production
callbacks and removes hci0 from sysfs.

Per speed: 27 HCI commands, 26 replies in 78 USB/HCC fragments, 54 bulk OUT,
101 bulk IN, 11 class actions, two management-info replies, two auto-off
acknowledgements and three drains. Both speeds also pass the pre-existing
seven startup cases. The HCI supervisor rejects 29 log and five artifact
mutations; startup rejects 26 and five. The all-module build links the test
consumer, owner, shared core and native HCI. SMP, KASAN, UBSAN, lockdep and
DEBUG_ATOMIC_SLEEP are enabled and checked; no warnings or sanitizer reports
occur. Checkpatch on the changed kernel code reports zero errors/warnings.
The first HCI attempt passed; no failed attempt is omitted.

This increment performs no physical USB operation: the physical owner and
protocol helpers are unchanged, and prior actual-U observations are not
relabeled as new qualification. Physical original-firmware startup, complete
physical HCI, E, real BLE peer/AP, BlueZ and RF/security/data qualification
remain open. Earlier KUnit, runtime/boot/download/loader and model-HCI results
keep their own source identities; they are not claimed as rerun here.

The SDK runtime-version audit binds four Host source files to clean reference
``d5e68a58fe38c2b04a16769c69de1e8a80e86d74``. Platform SDK/ROM print strings
are hardcoded; Wi-Fi Host and generated version constants differ. Neither
establishes the expected runtime reply from the pinned images. The fixture
firmware version must not become a physical whitelist. Audit receipt:
``runtime-version-source-audit-20261017.json``, SHA-256
``84d2f9a3fb365c93966a688be3a786214e99310d03d7f384be5c89b468ac9b58``.

Local records and manifest SHA-256:

* ``virtual-usb-build-20261017T124147.440651Z`` (PASS),
  ``cec83434ba62fd65a7f6b11c5766e51e43037b4e08a9f7fd0fbbabdccab948b3``.
* ``virtual-usb-20261017T124424.865630Z`` (PASS),
  ``dab85a21fb625f99954ca0bb37059cfc77ce7aafa7e9b6354536f860029a6bc2``.
* ``virtual-usb-20261017T124603.282200Z`` (PASS),
  ``03d47f8e309f6773ff54a3f753e4f0c382b2bd477aa7cdb1d92cfa4d12ea1ce7``.
* ``virtual-usb-negative-20261017T124604.322647Z`` (PASS),
  ``1e9c10a286d2a273edf5ec97a123508140679b9228b6ac2b4e04ac6bf7a2412a``.
* ``virtual-usb-20261017T124735.178653Z`` (PASS),
  ``719e6aed45368eac2ee5012924d63a1d50be01527c73ce982ad72e5e815c1ddc``.
* ``virtual-usb-20261017T124743.284795Z`` (PASS),
  ``31d7fde5f4efa350262f9679240d4ec6b4b372738140787ecd66e881a6a13a00``.
* ``virtual-usb-negative-20261017T124834.435094Z`` (PASS),
  ``13690adb315f7c8afd746e082ecac4688f7b377f09b83104a554f486a254a672``.
* ``hci-build-module-20261017T124833.313390Z`` (PASS),
  ``005dcbc8f5ade683cd17206c098f0cf022f6177be9813f1d807a57d98427be83``.

A subsequent supervisor audit tightened pending-Reset cancellation at clean
source ``a4d9f9e45d9d112c17f960bef0ae09215b50e315``. Teardown completion alone was insufficient:
the captured kernel disconnect/drain timestamps must differ by less than one
second, and any Bluetooth command timeout in the kernel log is rejected.
HCI_CMD_TIMEOUT is two seconds in this source. Two new negative controls
cover late drain and a command timeout, giving **31 log plus five artifact**
rejections. Fresh full/high HCI runs both pass; pending-Reset drain takes
**18,305 / 17,622 microseconds** respectively. This is a bounded fixture
cancellation check, not a physical performance guarantee. Kernel C and the
fixture are unchanged; the previous module/startup results retain their
``8877b3bcf78b`` source identity. No failure was found or omitted in this audit.

Additional records and manifest SHA-256:

* ``virtual-usb-build-20261017T125202.223036Z`` (PASS),
  ``5efb6d14321c4fb1e15c0b93965bc8927e72cef0532088e892cec8f534e8e211``.
* ``virtual-usb-20261017T125237.935315Z`` (PASS),
  ``f3f73403694de9365fe1a8e0a9385b481ff2f18944b1c40fc129d3d4cb971827``.
* ``virtual-usb-20261017T125246.200346Z`` (PASS),
  ``d3136f8b526ca20a4ed6d62849640dcb7cec58d2046814f837bfd81ea97b454c``.
* ``virtual-usb-negative-20261017T125309.383175Z`` (PASS),
  ``7167654bd15afc5d103035f06cb2a5d0f35041770311c75c148237812b01b04a``.

Allocated-U RAM trial: preparation and failed first physical attempt
------------------------------------------------------------------

Compiled and physically attempted source:
``7036cefcb555c15910bd01892a6c63f83af23a0b``. The external test module and
instrumented x86-64 KVM kernel build with four allocated jobs. The trial VM
has two vCPUs, 1 GiB, no disk/network and only the individually allocated U
passed through after identity/topology/ownership checks. SMP, KASAN, UBSAN,
lockdep and DEBUG_ATOMIC_SLEEP are enabled. Loading the external trial
module produces the expected out-of-tree taint inside the VM; it is not a
host-module operation or a kernel warning.

The explicit SDK110 reference inputs are XO coarse/fine 8/60, temperature
compensation off, and the fixed USB RAM preparation. All three original U
files are validated and supplied separately; no combined or E fallback is
installed. This bounded development selection does not qualify production
board defaults, RF calibration or cross-board compatibility. There is no
BLE/Wi-Fi enabling, eFuse/flash writing or hub/controller operation.

Preparation attempts are retained: the initial configuration rejected an
unresolved USB_XHCI_PCI dependency; USB_PCI/PCI_MSI resolved the build. The
first empty VM lacked DMIID sysfs, fixed in 7036cefcb555. Another launch could
not bind the local QMP socket inside the sandbox. The following empty VM
lacked the initramfs /dev/kmsg node; adding that node to the private supervisor
allowed the empty-VM preflight to pass. None of these attempts attached a
physical device. The trace verifier passes a synthetic positive and rejects
17 corruption controls; those are verifier checks, not device evidence.

The actual attempt at 13:14:26 UTC failed before loading. Guest usbmon has
exactly one bulk OUT submission, the 19-byte ``READM 0x40019000 4 ``. After
2,021,855 microseconds its cancellation completion reports status -2 and
actual length 19; the owner reports -110. The failed completion cannot be
interpreted as successful command receipt. There is no bulk IN submission,
trial rebind, RAM write, FILES transfer or QUIT. The supervisor originally
waited for runtime observation until its 30-second limit; it now detects the
terminal observation failure immediately. No device retry was executed.

QMP device removal, one callback drain, VM exit 0 and absence of a post-exit
host owner are independently checked. No BUG/WARNING/sanitizer/lock report
occurs. The original FAIL manifest remains unchanged; a separate
failure-analysis.json verifies cleanup without relabeling startup as PASS.
The boot observer and USB IO source are unchanged from the preceding
successful physical observation at 97b978a49a0b. The root cause remains
unresolved; this evidence cannot distinguish device state from passthrough
behavior. The user has been asked for a new single-device power removal
before another attempt, which remains pending at this checkpoint.

This attempt has no new cold-start confirmation. The earlier confirmed cold
U read keeps its original scope. Original-firmware physical loading/runtime
comparison, complete physical HCI, E, real peers/AP and RF qualification
remain open. No external submission or support milestone is claimed.

Records and manifest SHA-256 (including all preparation failures):

* ``hardware-build-20261017T130231.470698Z`` (FAIL),
  ``08a5032abd071798e86d1ce644e7b31f0316b30ba075df5084c8948eed56e7c6``.
* ``hardware-build-20261017T130451.999801Z`` (PASS),
  ``f0a1602b2a9da9a24635e32bc5a820cb23c8c01751015b372bf241dc1d234896``.
* ``hardware-build-20261017T130920.618349Z`` (PASS),
  ``ed3a66c211116ba87902d54019b8480d3e321aed23a8b3a2c9c5158fabed9a11``.
* ``ram-trial-20261017T130853.776180Z`` (FAIL),
  ``2b15623886e3eb7e6284a440ad4e00fb39f8e1cfc4793d136c6a1ac327e19f21``.
* ``ram-trial-20261017T131330.253447Z`` (FAIL),
  ``5a9be5cac3643dce9cbddbae14146efb2ef26bc991c7e6aa204733078aba5463``.
* ``ram-trial-20261017T131346.413504Z`` (FAIL),
  ``e38f6ab1a560e10bf84188c3ea21946e32f9182687a0e9aa211205aa9b8d7f08``.
* ``ram-trial-20261017T131410.962393Z`` (VM_PREFLIGHT_PASS_NO_HARDWARE),
  ``cead38294cd967bbed3c1d469bdc9f022c464ea6a3e7f5f2a9bd49721b2291b9``.
* ``ram-trial-20261017T131426.761683Z`` (FAIL),
  ``9899dcc14c2a3562ac1e071a306490e3a4cbc04ca0b6e7e29fb5958ce6385edc``.
* ``ram-trial-verifier-20261017T131041.905859Z`` (PASS),
  ``295ebd1271d68a12589caf2b654d0a16ad2398917233e664462e95fb33eb5d78``.
* ``ram-trial-verifier-20261017T131330.052044Z`` (PASS),
  ``295ebd1271d68a12589caf2b654d0a16ad2398917233e664462e95fb33eb5d78``.
* ``ram-trial-verifier-20261017T131623.112434Z`` (PASS),
  ``690e26b31d4079622043976ba2b0297b60b90ef03d6d6b77a533621095b860c3``.

Separate failure-analysis SHA-256: ``2e11ef4b37bb5ac2b14a15c5a9c7b512db5e9269aa3f76d142bf6fafc2f8fb65``.

Wi-Fi framing/control audit and compiler layout checks
------------------------------------------------------

Clean probe source: ``a26a1504ba4b``. The follow-up Wi-Fi interface audit
byte-compares 23 source files against SDK110 commit
``d5e68a58fe38c2b04a16769c69de1e8a80e86d74``. It distinguishes control and
frame services/lengths, traces Host scan and channel switching, identifies
an SDK channel-error path that returns success, and follows the asynchronous
TPC table submission. These are Host source findings, not device behavior.

The independent probe compiles the actual pinned FRW declarations and fixed
sentinels for x86-64, i386, arm64, RISC-V32 and big-endian PowerPC64, then
extracts object sections without executing any compiled code. All five
match their independently specified byte vectors. The communication envelope
is 28 bytes on all targets; the pointer-bearing native structure is 28/40
on 32/64-bit, and big-endian integer/bitfield layouts differ. This is not
proof of the original firmware compiler ABI or a big-endian driver build.

The first attempt failed in ccache before compilation because its cache was
not writable. Disabling ccache resolved it. The subsequent development run
and clean committed run pass; their separate manifests preserve source/tool
hashes, exact bytes and all private SDK excerpts. Those excerpts and objects
are not uploaded. No kernel/runtime test or new physical operation occurred;
the allocated-U power-cycle request remains pending. Prior KUnit and hardware
results retain their original source IDs. No Wi-Fi capability is registered.

See :doc:`ws73-wifi-interface` and ``tools/testing/ws73/wifi/README``.
Private manifest SHA-256 values:

* ``wifi-framing-control-audit-20261017`` (STATIC_HOST_SOURCE_AUDIT_NOT_DEVICE_QUALIFICATION),
  ``4ae3b2bf157ec66955cab896447b1bf19aa33ad671b5a82e22ffa296009159b8``.
* ``wifi-abi-20261017T132401.745338Z`` (FAIL),
  ``1ed98181efeba025335dfd020bbf0363655f2bc41872b391b0f7db6e799a39be``.
* ``wifi-abi-20261017T132420.920684Z`` (COMPILER_LAYOUT_CHECK_PASS_NOT_FIRMWARE_ABI_PROOF),
  ``941b330766d6227d07dcc7fab7b6d9d11ea94e9a437fe02c32500e852b1af6d3``.
* ``wifi-abi-20261017T132706.912949Z`` (COMPILER_LAYOUT_CHECK_PASS_NOT_FIRMWARE_ABI_PROOF),
  ``66866b091c0e25449271cc33b803d74108309c8f8befbc2e82ce4b69ae035fc1``.

The probe was then tightened at clean source ``9e616107634f`` to verify each
object's ELF class, byte order, relocatable type and machine ID independently
of the requested compiler target. All five targets pass again. The earlier
records retain their original scope; this verifies target objects, not a
cross-architecture kernel build. Record
``wifi-abi-20261017T132844.086882Z``, manifest SHA-256
``5b5f8d469c82576b8f9a20f92e5da44e40568183db49c487fff3201b586a790d``.

One-shot admission for the physical RAM trial tool
--------------------------------------------------

Clean built/tested source: ``44fbb6c8b6890f9da262e122c677b648a37c75c2``.
Review of the USB core matching path showed that absence of a module alias
prevents autoloading, not matching after a driver is registered. The earlier
trial registered before observing identity and had no module-lifetime probe
limit. No unintended bind was observed in the failed physical attempt; that
attempt never passed the normal observer. This is a trial-control correction,
not a diagnosis of its READM timeout.

Guest PID 1 now waits for identity/ROM observations, checks the device number
captured from the boot enumeration and rechecks observer ownership before
loading the trial. The module requires that number as well as the existing
DMI/serial/port/descriptor checks. Probe requires global PID 1 and consumes
an atomic admission before calling the shared bind. Automatic worker probes,
a different device number and a repeated probe are rejected; even a failed
first bind cannot replenish admission. Re-enumeration does not reset it.
The production USB owner and firmware loader have not changed.

The separate tagged-VM test module runs eight pure admission cases: automatic
context, wrong enumeration, zero/out-of-range numbers, first admission,
repeat, changed enumeration after admission and another repeat. Guest init
also attempts module loading without a devnum and requires ENODEV, then
requires the trial driver to be absent before READY. These checks do not
register a USB driver or execute firmware loading. The trial module is loaded
with valid inputs only after future actual observation checks pass.

Four-job build and empty KVM preflight pass. The supervisor sees eight cases,
exactly one no-driver readiness marker, only qmp_capabilities/quit commands,
VM exit 0 and no kernel/lock/sanitizer warnings. The separate external test
module gives the expected out-of-tree taint. No physical device was attached,
no real probe/disconnect was exercised and no further cold or startup evidence
is claimed. The single-device power-cycle question is still pending. The
changed kernel C has zero checkpatch errors and one new-file MAINTAINERS
reminder; the formal-series maintainer-entry work remains open.

Private records and SHA-256:

* ``hardware-build-20261017T133744.285944Z/manifest.json``,
  ``c0682cbd49e8ed4c7f8f6fb4643cad15bae9695510f1e610ecdae076c90760c6``.
* ``ram-trial-20261017T133810.412175Z/manifest.json``,
  ``22113b5a06d2795ff92d21cf95f4ee9ddb511878fdce6fe95228373b44010076``.
* ``ram-trial-20261017T133810.412175Z/guard-analysis.json``,
  ``ca2ec6f5f887db072dd973568d68dfc817b46be7dc9e982972cb293454324a79``.

Bounded USB byte mutation and H4 state sequences
------------------------------------------------

Clean VM-tested source: ``3bed68629b6524c53902532359546369751393e0``.
The existing USB aggregate truncation checks often supplied a larger backing
allocation than the declared length. New mutation tests use an unaligned input
with exactly the requested allocation extent, so the KASAN boundary follows
that input rather than a large reusable scratch buffer. Four fixed xorshift32
seeds generate 16,384 inputs from valid BLE/platform/notification aggregates,
including short/large records, bit mutations, length/table changes, truncated
inputs and trailing bytes. The production ws73_usb_aggregate_decode is called
directly. Checks cover immutable input, unchanged output on failure, and
bounded contiguous borrowed records/notification values on acceptance.

The production H4 reassembler separately runs 16,384 state steps with the
same four seeds: valid fragments, malformed/random input and explicit resets.
An independent accumulator compares complete output byte-for-byte with the
consumed input across partial feeds. Failed streams must remain byte-identical
and reject input until reset; partial/error calls cannot publish packet output.
The test is serialized and does not prove concurrent RX drain or lifecycle
behavior. Its generated HCI bytes are synthetic, not controller replies.

All **83 KUnit cases in 14 suites** pass with SMP, KASAN, UBSAN, lockdep and
DEBUG_ATOMIC_SLEEP. USB totals are 3,533 accepted / 12,851 rejected inputs.
H4 totals are 2,211 completed / 1,870 partial / 1,266 first errors / 9,009
terminal rejections / 2,028 resets, totaling 16,384 steps. All four seeds
exercise each required category. No kernel warning, sanitizer or lock report
occurs. Checkpatch reports zero errors/warnings on changed kernel test code.
The build emits the existing Documentation/.renames.txt gitignore warning,
which is not a compiler or runtime diagnostic. Four build jobs were reserved
and the two-vCPU VM had no hardware passthrough or network device.

The first build at c7ae363e945f failed because a KUnit pointer assertion was
passed an array expression. Its FAIL record is retained. The follow-up changes
that expression to an element address; the subsequent build/VM run passes.
No failed runtime input or minimized production-code counterexample was found.

Supervisor source ``9db3aaeb29f3`` additionally requires all four seed counters
for each applicable suite, exact 4,096 totals, positive coverage categories,
and no malformed/duplicate/unexpected marker. Its parser was checked against
the retained actual VM log: three positive counter views and twenty negative
controls pass, including missing/duplicate seeds, wrong totals, zero categories,
truncation, old case-name-only text, trailing corruption and unexpected modes.
These are counter-parser tests, not fresh core-only/HCI-only VM runs. Kernel C
is unchanged after 3bed68629b65; its VM evidence is not relabeled to the later
supervisor source.

This is bounded deterministic mutation, not coverage-guided fuzzing, arbitrary
state-space coverage, KCSAN or formal proof. The physical READM failure remains
unresolved and the new power-removal confirmation is still pending. No physical
USB operation, firmware load, RF or support-qualification milestone was added.

Records and manifest SHA-256:

* ``20261017T134907.771248Z`` (FAIL),
  ``8deb275401b14d86758b650eee7514197af8bc9ebb9a676895a43380549e29c9``.
* ``20261017T134954.268196Z`` (PASS),
  ``1345f905e49657a89eb9b8ac41522a795f2c0bd0ceceec42a5e8364e0d4cc5c4``.
* ``mutation-supervisor-20261017T135303.863717Z`` (PASS),
  ``e928b2a8062bcf2ae4f7ade1099d14a720b0dec7f0124e9cc3527841729d5e87``.

Actual driver cross-architecture compile and link matrix
-------------------------------------------------------

The earlier Wi-Fi ABI object probe did not compile the actual driver. The new
``tools/testing/ws73/build-cross.py`` compiles vmlinux first, then the shared
core, USB I/O/owner, native HCI, H4 protocol and Bluetooth stack modules.
Profiles are arm64 little endian, i386 32-bit little endian and PowerPC64 big
endian with Clang, plus x86-64 with GCC. PowerPC64 uses the supported ELFv2
kernel ABI. Arm64 big endian depends on BROKEN in this tree and is excluded.
No KUnit, virtual USB or hardware trial modules are enabled in this matrix.

The first four builds at ``8e8fc64252e82f7e5e36edbdbc2fe7aa432fd99d`` pass,
but configuration review found that allnoconfig disables BT_LE. These records
are retained as minimal-HCI compile results, not LE-stack coverage. The
follow-up at ``1709ab9d18ced4906afee5f5ecc0b18c81511fe7`` explicitly requires
BT_LE=y and BT_BREDR=n. All four follow-up builds pass too. They are incremental
builds in the corresponding fixed per-profile directories; no production C
changed between these two sources. Their earlier full-build records remain
part of the provenance. Each profile runs sequentially with four jobs and the
shared build lock, after checking whole-machine resource use.

Every build checks resolved module dependencies, and completes vmlinux and
module links without unresolved symbols. The archived kernel and six target
modules have the expected ELF word size, byte order, type and machine ID.
Configuration, source, tools, command log and artifact hashes are retained;
a separate audit verifies all artifact hashes, configuration modes and ELF
headers, and saves llvm-readelf output for shared-core/native-HCI modules.
All builds use W=1. No WS73 source warning occurs. The first arm64 build has
517 initializer-overrides warnings in architecture syscall/trap tables, and
the first GCC build has one unused-but-set-variable warning in
fs/proc/proc_sysctl.c. These are retained, not hidden by the subsequent
incremental logs (which contain no warnings). Compiler versions are pinned
in each manifest. This is not a whole-kernel zero-warning claim.

Only the modular BLE consumer plus its shared USB dependencies is qualified
for compilation here. Built-in/module permutations, cross-architecture runtime,
module insertion/removal, Wi-Fi, PM and physical behavior are not established.
No kernel/module was installed or executed, and no hardware was accessed.
The physical READM failure and pending new single-device power-cycle
confirmation remain unresolved; prior physical observations retain their
original scope. No formal support or upstream acceptance gate is closed.

Private records and manifest SHA-256 (first round BT_LE disabled, second
round BT_LE enabled for each profile):

* ``cross-arm64-clang-20261017T140302.019714Z``,
  ``6a2c081f7349b309f56f5202cb7dd372d62df364e7349d952cf116f7c02f9a51``.
* ``cross-arm64-clang-20261017T140934.630843Z``,
  ``99249bf9aaed09aa3c1673b2517dd045232889551bb5a2de2bb22f1458603a38``.
* ``cross-i386-clang-20261017T140442.559379Z``,
  ``58c81751811f897c7ace376d494f13861b255f6d4cecf06c0f2254095e501982``.
* ``cross-i386-clang-20261017T140949.075009Z``,
  ``72a3f817e6134609dead72c8ba736706130742c2356d23dd2677726729bbbe82``.
* ``cross-ppc64be-clang-20261017T140614.346468Z``,
  ``c5e470b514b6439f63c898715f499d4a02f5f5030c53fd9efcf0fea7da61d571``.
* ``cross-ppc64be-clang-20261017T141008.453016Z``,
  ``40ace5feea06a0f6aa203c400501e839ade58f1a35c654196f236795fc1ef889``.
* ``cross-x86_64-gcc-20261017T140739.378910Z``,
  ``82764608f6fe61bdf438e6114bba7e78c6ee28b9807907254c5e03c1c523f7de``.
* ``cross-x86_64-gcc-20261017T141023.929702Z``,
  ``fdbd201e28222592dcbdac5968f907e74caf180e2e70e1180ec052c6bc552e47``.

Artifact audit ``cross-matrix-audit.json`` SHA-256:
``f93d102bcef6e7de886b30ff71180e807302dc769a18fd438d5eec7e4d473632``.

Strict KCSAN runs and synchronized native HCI fault snapshot
----------------------------------------------------------

The native HCI fault worker read bt->error with READ_ONCE while remove could
update the same field using a plain store under bt->lock. Source review found
this inconsistent access discipline; no KCSAN report reproduced this specific
WS73 race. At ``ffd7b1fc3f16bf4d8be7bdad29fe6e5c2b29a2d7`` the worker takes one
snapshot under the same state lock and uses it for both shared fault delivery
and native command cancellation. The lock is released before callbacks and
HCI operations. Remove/fault-work draining and terminal behavior remain intact.
Checkpatch on the production C change reports zero errors and warnings.

The runner now offers --kcsan in a separate build-kunit-kcsan directory.
KASAN is disabled as required by Kconfig; UBSAN, SMP, lockdep and
DEBUG_ATOMIC_SLEEP remain. Strict checking, weak-memory modeling, early
enable, boot selftests and KCSAN_SKIP_WATCH=1000 are required. The supervisor
checks activation/strict-mode/3-of-3 selftest records and actual compiler
memory-access hooks in shared BLE endpoint/link objects and native HCI when
enabled. Kernel diagnostics reject PASS independently of KTAP case statuses.
All VMs have two vCPUs, 1 GiB, no network and no physical USB. Four compilation
jobs were reserved; another 12-job build was observed and allowed to finish
before the first build started. Other project VMs were not operated.

Successful bounded executions:

* Shared core/protocol: 83 cases in 14 suites under KCSAN at
  ``fe0ece87d031a5ef70aec14c0e086cb4a99a818d``. The native HCI consumer is off.
  This includes independent-receive-thread control tests and bounded parser
  mutations. The following production change touches only btws73.c, which
  is not part of this shared-core test configuration.
* Native HCI/protocol: 31 cases in 3 suites under KCSAN after the fault snapshot
  change at ``ffd7b1fc3f16bf4d8be7bdad29fe6e5c2b29a2d7``. Existing tests cover
  blocked TX, concurrent flush/RX, local/shared faults and removal against
  explicit synthetic backends. They stop before complete HCI initialization.
* The same 31 cases pass separately with KASAN, UBSAN and lockdep at ffd7b1fc.
  The two modes share codec cases; they are not 114 distinct tests.

No kernel diagnostic occurs in those three successful runs. The initial
completed HCI KCSAN run at baa3e8653ff9 also passed 31 cases, but used the earlier fixture
and is not relabeled as the final configuration. All build logs are retained:
full rebuilds include existing kernel-doc warnings outside WS73 and the
Documentation/.renames.txt gitignore warning. No WS73 compiler warning occurs.

Failures and limits are part of the qualification result:

* The first sandboxed run built successfully but QEMU could not bind its QMP
  Unix socket. No test ran. The terminated attempt remains FAIL; the next
  authorized VM execution used the same independent output directory.
* The first shared-core KCSAN boot reported ``ps2_do_sendbyte / ps2_interrupt``
  in the QEMU i8042/PS2 mouse path before any KTAP test. panic_on_warn remained
  enabled; the supervisor terminated the panicked VM at its 120-second limit.
  The headless serial fixture now disables KEYBOARD_ATKBD, MOUSE_PS2 and
  SERIO_I8042. The first configuration attempt disabled only i8042 and was
  rejected because the keyboard/mouse selectors re-enabled it; no VM ran in
  that attempt. Failed preflight records may retain earlier build artifacts;
  they are not new execution evidence. This fixture isolation does not fix
  or qualify the PS/2 path.
* A later HCI KCSAN run completed five cases, then reported an unknown-origin
  race in ``blake2s_update`` through ``release_task / add_device_randomness``.
  It panicked and reached the same 120-second limit. Local source shows
  release_task passes p->se.sum_exec_runtime by address to randomness mixing;
  concurrent runtime accounting is a hypothesis, not a confirmed writer.
  This report remains unresolved. No report filter, relaxed KCSAN mode or
  change to the random/exit code was added. The next run was justified by
  the separate WS73 snapshot fix, not repeated attempts to obtain a pass.
  Its success does not close this report or prove KCSAN stability.
* Supervisor negative checks initially accepted two trailing-corruption
  records. Exact complete-line checks fixed that; the final 33 negative
  controls reject missing/duplicate/malformed markers, wrong selftest counts,
  missing configuration, diagnostics, and both actual failed KCSAN logs.
  Positive inputs are retained actual VM logs. Intermediate 25/28/31-control
  records remain with their original result and source.

**Overall KCSAN qualification remains incomplete.** These bounded sampled
executions do not prove race freedom, all interleavings, complete HCI/BlueZ,
physical USB or firmware behavior. The normal physical driver is still
observation-only; no hardware was accessed in this increment. The new
single-U power-cycle confirmation, original firmware startup, E hardware,
BLE peer/AP and formal support/upstream review gates remain outstanding.
No unrelated kernel report was submitted externally.

All VM/preflight and supervisor attempts, manifest SHA-256:

* ``20261017T141527.263871Z`` (FAIL),
  ``4b513b9522e3d41d70a2eefcaf69cd2bf1c733c77c9fe9c532c14a48e4fbd489``.
* ``20261017T141823.934481Z`` (PASS),
  ``922c2e957b0b090815b177c16f8909bcfdd8943090343a7c190dbc87344cc4e7``.
* ``20261017T141957.468777Z`` (FAIL),
  ``f3f54d8fe062ced9bb2feeb894f1d45a0465bf7f4d62c35fe2895fc7d85c6d8f``.
* ``20261017T142627.501310Z`` (FAIL),
  ``bced9107dbe06ba79a2f6dcfc61313d51eed450b219ef242f3e98305af944de0``.
* ``20261017T142724.346206Z`` (PASS),
  ``49cb36995701cc1cddf81d4784836fd543ff67ab82f2db715f0fdd3ef16b2b0f``.
* ``20261017T142850.700223Z`` (FAIL),
  ``9a4b830071565ed7e71380935f879d315a25976b6bc905ee22afee939d00b247``.
* ``20261017T143506.386391Z`` (PASS),
  ``ec1536df84c34ae84516654d292f117401045710153d6afe010ec3139624b83f``.
* ``20261017T143540.546882Z`` (PASS),
  ``d56b0f24a7a6c8050c3fedeb2a3857a9ab328ecda86fa2b7bd5bc1a0edb0f95c``.
* ``kcsan-supervisor-20261017T141850.322356Z`` (FAIL),
  ``08e721a69782bd425837d9f0f0e5a35a3e5ab418444df7ea04f523eeecb02ed4``.
* ``kcsan-supervisor-20261017T141918.677630Z`` (PASS),
  ``200d63836625d6c2a15ee603706a04549a9895e91197402f69b38d545149f154``.
* ``kcsan-supervisor-20261017T142845.751192Z`` (PASS),
  ``08c4a3ecbb68f7cd190b218f262844be47a7fd26fe38361ff0f841fbcdc8ca16``.
* ``kcsan-supervisor-20261017T143541.724583Z`` (PASS),
  ``0d56fad27fa40ba224557c19bc3eba751895e8c9bcb6606fb82a5fa71c6be99e``.
* ``kcsan-supervisor-20261017T143618.558353Z`` (PASS),
  ``fe41a851dbf9cbb6ba723236e1d6da02c81fac1ede6ff752d018c741a9f3ce35``.

Artifact/source audit ``kcsan-audit.json`` SHA-256:
``31804af300a09ec6b84e0e52f3858e031ed997ee904176f5f9f09c9854a403a9``.

Bounded KCSAN writer diagnostic: stopped without resolving the writer
-------------------------------------------------------------------

The previous randomness-path report was investigated against source
``8219a8d965422a3c1790d5ac45b53ea41a9a1b62``. release_task passes
p->se.sum_exec_runtime by address, and fair scheduler update_se updates that
field. kernel/sched/Makefile disables ordinary KCSAN access instrumentation
while retaining barrier instrumentation. This supports a candidate explanation
for an unknown-origin report, but does not identify the writer in that run.

A temporary VM-only patch added __kcsan_check_read_write at the actual
update_se accounting write. Its dirty patch, original/modified source,
compiled fair.o and command file, symbol inventory and kernel artifacts are
retained. The object contains __kcsan_check_access. The experiment was bounded
to at most three VMs and would stop on a relevant report or unexpected failure.
The first VM instead reported ``kthread_is_per_cpu / worker_thread`` before
any WS73 KTAP case, panicked, and was terminated by the original 120-second
supervisor deadline. This is a report in the temporarily instrumented kernel,
not a newly claimed reproduction on an unchanged baseline. The temporary
instrumentation may perturb timing; no causal claim beyond the trace is made.

The experiment stopped after one attempt, without suppressing reports or
trying to obtain PASS. Its precise source restoration was checked against
HEAD and the saved original hash. No scheduler or exit/randomness changes
were committed, and no external report was submitted. The original randomness
writer remains unproven; the new startup report is also unresolved. Existing
archived clean runs retain their original scope. The reusable KCSAN build
cache contains this diagnostic build until rebuilt by the runner; it is not
a clean qualification artifact.

* ``20261017T144617.720728Z/manifest.json`` (FAIL),
  ``5048fe75cb62d96dd56846affea1756e66477266d5c5897e3d8255a6a18e4d7f``.
* Temporary scheduler patch SHA-256:
  ``042a2d007c057f17aba0333559d026a8a9d5b1fc96d0a4a7bffd2bc4fe57f194``.
* ``20261017T144617.720728Z/diagnostic/manifest.json`` SHA-256:
  ``851fce7a01443e9ef821a30d82ac7ebb431c88962260a7c5e9a84777e368e605``.

Conditional CBMC verification of production HCC scalar converters
----------------------------------------------------------------

Final checked source: ``f03ce8f3451f`` (full commit and tool identities in the
manifest). tools/testing/ws73/formal/hcc.c includes the actual production
hcc.c and public ws73.h rather than copying its algorithms. The four checked
entry points are chip/version query encoding and chip/version reply decoding.
The aggregate decoder in the same translation unit is outside this proof.

CBMC 6.11.0 checks i386 and x86_64 scalar/pointer models. Length/capacity ranges
over the full size_t domain; bytes, NULL arguments and previous output values
are symbolic. Allocations model valid retained, distinct caller objects and
an unaligned start, ending at min(declared extent, inspected prefix). Their
18/24/32-byte prefixes and leading guard establish that shorter extents are
respected and longer suffixes are not accessed. Caller allocation failure is
excluded; protocol NULL inputs/outputs are not excluded.

The trusted shim covers kernel scalar types, errno values, module metadata,
memset declarations and bytewise unaligned little-endian helpers. The actual
kernel helper implementations are not proved equivalent by this harness.
CBMC/frontend/library/preprocessor correctness and caller ownership assumptions
remain dependencies. Success means the stated C conversion properties under
these assumptions, not hardware protocol authenticity or compatibility.

Eight positive jobs pass: each architecture reports 443 chip-reply and 395
version-reply properties; query jobs report 210 each on i386 and 209 each on
x86_64. The total is 2,514 successful property results across the jobs, with
shared checks repeated; it is not a count of unique tests. Exact acceptance,
query bytes, failure-output preservation, unchanged input, successful value
preservation and pointer/bounds/arithmetic/conversion/leak checks pass.
Each job obtains a complete --show-properties inventory first; every solver
result ID and description must match it, in addition to explicit contracts.
Unwind 33 and unwinding assertions cover the 32-byte query loop.

Five negative production-code copies yield exactly the intended counterexample:
wrong chip query tag, wrong version request role byte, premature chip output
publication, removal of the declared record-length bound, and returning the
firmware word as the ROM word. Those copies are private proof inputs and never
kernel code. Another actual solver run with unwind 1 fails an unwinding
assertion and is rejected as incomplete. The result supervisor also rejects
24 damaged-result controls, including missing memory/contract properties,
unknown/failed statuses, warnings, mismatched inventories, abnormal exit,
truncated JSON and invalid negative-control outcomes; all 13 retained positive
and expected-counterexample records validate.

The initial preparatory probe failed because host 32-bit libc headers were
absent. A second single-function development probe succeeded. Those probes
were uncommitted, did not retain complete source snapshots, and are not
qualification evidence; their raw commands and output remain. The first
committed matrix also stopped when CBMC's builtin memset model included the
host string.h. A minimal standard string declaration now isolates that model
from host multilib headers without changing the production implementation.
The failed full matrix remains NOT_PROVEN, even though two jobs had succeeded.
The subsequent complete matrix and final inventory-checked matrix both pass
with their own source hashes. Each solver runs serially under the project
lock, with a 60-second and 4-GiB address-space limit.

No kernel or driver was executed on the host for these proofs. Production
WS73 code is unchanged in this increment. USB aggregation, H4 streaming,
concurrent lifecycle/refinement, TLC/LKMM, liveness and real devices are not
proved. KCSAN reports, original-firmware physical startup, the pending new
single-U power-cycle confirmation, E hardware, peers/AP and upstream review
remain open. This is the plan's conditional bounded-C evidence category,
not completion of the formal verification or hardware qualification program.

Records and manifest SHA-256:

* ``cbmc-hcc-20261017T150007.939157Z`` (NOT_PROVEN),
  ``4e93240c234e1fed74c7762c2ad246a881e7e5260b74b237731c6e43a8f9a950``.
* ``cbmc-hcc-20261017T150040.967709Z`` (BOUNDED_PASS),
  ``40a7478aa7fb6a1d0b1052702260fd7601840c3eca23dbd001523dd76058562c``.
* ``cbmc-hcc-20261017T150252.752024Z`` (BOUNDED_PASS),
  ``01dffc9088d12067c1ae8475f1889e82f9620a1d81c771bf467491a8061f0663``.
* ``cbmc-supervisor-20261017T150434.923169Z`` (PASS),
  ``3434c5ed819d7c5933444f9e26bd159d1d98cd89ba2d97b05a07fa0fdf26489e``.
* ``cbmc-unwind-control-20261017T150612.759243Z`` (EXPECTED_INCOMPLETE_REJECTED),
  ``f27a10bd1d401bbd999e394b0940bad99f2f7f37d28ef5ccd64da4c6e3b9b96f``.


USB stop-work producer drain and finite TLC model
------------------------------------------------

A source audit found a concrete teardown scheduling window in
``ws73_usb_unbind``. A BLE backend holding ``ble->operation`` can enter
``ws73_usb_fail`` before disconnect, set the first error under ``ws->lock``,
and be delayed before ``schedule_work(&ws->stop)``. Disconnect previously
cancelled stop before removing the child. That cancellation could return
before the backend queued stop; later child removal joined the backend but
left queued work referring to the freed USB owner.

Production fix ``a1124154241cfb3f825d79c5348f86b5d2fd86b5`` moves the final
stop cancellation after child removal. RX/interrupt and initialization are
already joined; child removal drains backend calls under the operation mutex
and removes their context. Only then is the final stop cancellation performed.
No lock needed by those producers is held across their joins. The code-only
strict checkpatch result is 0 errors, 0 warnings, 0 checks. The original
commit-message check additionally reports one long-body-line warning; both
logs are retained for review-series cleanup, without rewriting development
history or claiming the message check is clean.

``formal/USBStop.tla`` maps admission, first-fault claim, delayed enqueue,
callback/initializer/backend return, stop execution and the actual unbind
sequence. Importantly, this kernel's ``usb_kill_anchored_urbs`` ALREADY waits
for completion callbacks through ``suspend_wakeups`` accounting in the USB
core. The model does not invent an anchor-empty callback UAF to justify the
explicit per-URB joins. Its old-order negative control instead exposes the
actual delayed backend enqueue window described above.

Final model and VM source: ``00681182e88db73f80e866e6f18fd769baa8a560``.
TLC 2.19 (rev 5a47802), one worker, 512-MiB heap and a 60-second deadline:

* Corrected order: 5,820 generated states, 2,058 distinct states, zero queued
  states at exhaustive completion, depth 21, and 16 reachable terminal states.
  TypeOK and NoUseAfterFree hold in this finite abstraction.
* Historical order: the required NoUseAfterFree counterexample has 15 states.
  Backend first-fault claim precedes disconnect; enqueue follows the old
  cancellation; child removal/free occurs while stop remains queued.
  Exit 12 is expected only with this exact invariant and schedule evidence.
* Fifteen malformed/missing/duplicate/wrong-exit/wrong-invariant supervisor
  controls are rejected. Reachable-state dumps and all model/config inputs
  are archived. A success banner without completed statistics cannot pass.
* The first committed runner attempt at ``ff79f154ab6a`` explored the model
  successfully but looked for a dump without TLC's added ``.dump`` extension.
  It remains NOT_PROVEN; correcting the path produced the final complete run.
  Earlier uncommitted probes include a sandbox socket denial and successful
  positive/negative explorations. Raw logs remain in ``tlc-probe``; these
  development probes did not archive complete input snapshots and do not
  grant qualification.

The graph bounds are one owner, one submission for each of RX/interrupt,
one initializer, one BLE backend operation and one first fault. There are no
state constraints or simulation shortcuts. This is a safety abstraction with
explicit lock/join/lifetime assumptions, not a C refinement proof or liveness
proof. Resubmission, synchronous request stack lifetime, full auxiliary/HCI
release, multiple instances, recovery, PM and physical behavior remain outside
this model. TLC's fingerprint collision estimate (4.2e-13 for the positive
run) remains in raw output. The drift guard checks current C teardown order,
not semantic equivalence to the model. No executing-kernel UAF reproduction
is claimed for this particular interleaving.

The affected actual USB/HCI path was rebuilt with SMP, KASAN, UBSAN, lockdep
and DEBUG_ATOMIC_SLEEP, then exercised in hardware-free KVM at full and high
USB speed. Each run passed three cases: full native initialization, removal
while Reset is unanswered, and a fresh successful initialization. Each saw
27 commands, 26 explicit synthetic replies in 78 fragments and three drains;
QEMU exited successfully. The full-speed supervisor rejected 31 damaged-log
and five bad-artifact controls. These tests execute production USB owner and
auxiliary/HCI code but do not deterministically force the source-derived
late-scheduling window. All recorded build/TLC artifacts and both VM material
hashes and observed summaries were revalidated before this report.

No physical USB was accessed this increment. The failed physical READM trial,
new single-U power-cycle confirmation, original-firmware startup, board/E
qualification, BLE peer/AP, Wi-Fi firmware gaps, KCSAN reports and upstream
review remain open. This fixes one real producer ordering defect; it does not
close #10, the formal verification program or any U/E hardware support gate.

Records and manifest SHA-256:

* ``tlc-usb-stop-20261017T152245.541240Z`` (NOT_PROVEN),
  ``583e951b5b1672c2842da3a96f2690d533fe4705e1a97342da9aba3764be188f``.

* ``tlc-usb-stop-20261017T152329.213136Z`` (FINITE_MODEL_PASS),
  ``cbcef28caff823b8b71064507d2c537b5c0e926c32d89f289b37a3b73f59c872``.

* ``virtual-usb-build-20261017T152303.233185Z`` (PASS),
  ``b42a57dc78088faf05024cf28ec57dde16ccab57ea32b57700737bb37aad04be``.

* ``virtual-usb-20261017T152341.883693Z`` (PASS),
  ``093b6800da36281707014c1087c0da5108f18afc300476b7192bd00a77ac9734``.

* ``virtual-usb-20261017T152423.310680Z`` (PASS),
  ``706f788c9e984e8fb91447cf2e35740f5a18c0de1e750e4877e46cd6c50b2e06``.

* ``virtual-usb-negative-20261017T152424.354078Z`` (PASS),
  ``03af8c34059f06a920328a6501e9939f1a585016557e8928ce24023b69a1789c``.


Wi-Fi descriptor declarations and key-transaction audit
-------------------------------------------------------

At source ``c0df1f02e8981f884ad9a391ee3d7c12197cbf24``, the extended
``tools/testing/ws73/wifi/check-abi.py`` byte-compared six SDK headers to
``d5e68a58fe38c2b04a16769c69de1e8a80e86d74`` and compiled their scalar
RX/TX-status declarations along with the existing FRW/control declarations.
All five targets passed: x86-64, i386, arm64, RISC-V32 and PowerPC64 big endian.
Recorded ELF class, endianness, machine/type, exact independently specified
sentinel bytes and size/offset assertions agree. RX info/status/statistics
occupy 20/4/8 bytes in the 32-byte control; TX status is eight bytes. The four
little-endian targets agree, while big-endian integers and bitfields differ.
The exact layout result is not original-firmware ABI or architecture support.

The single new matrix attempt passed; no object was executed and no kernel
was built or run for this increment. Original pack scopes are modeled around
extracted declarations; compiler layout and this extraction are the evidence
boundary. Neither the two-bit RX tid field nor latency fields establish a
complete QoS TID or CCMP PN export. The status declaration alone contains no
retry/rate report. All artifact hashes were rechecked after execution.

A further source audit byte-compared 16 Host SDK files at the same pin and
recorded 11 findings. It traces HAL key setup to multi-register operations,
void error-reporting wrappers, FRW writes with discarded errors, all-ones read
error sentinels, a possible 16-bit write fallback, PN clearing, global replay
control and a polling counter that excludes register-transaction time. The
CCMP helper only reports IV length; it is not a software CCMP implementation.
These findings constrain the future driver transaction design; they are not
claims of a verified firmware exploit or absence of all alternative firmware
interfaces. Exact call-chain details and implementation consequences are in
``ws73-wifi-interface.rst``. No SDK source excerpts or objects are added to the
kernel repository; they remain in private records.

Production kernel code is unchanged. The existing HCI/USB runtime tests were
not rerun for this probe/documentation change and retain their exact source
identities. The physical READM failure, new power-cycle confirmation, original
firmware startup, board/E, peer/AP, ordinary-data completion, RX security,
regulatory application, KCSAN and upstream review gates remain open. No Wi-Fi
adapter or speculative firmware capability is registered.

Records and manifest SHA-256:

* ``wifi-abi-20261017T153238.893347Z``
  (COMPILER_LAYOUT_CHECK_PASS_NOT_FIRMWARE_ABI_PROOF),
  ``763a2085d1002a8e3290bd79e1eca335753ece1b9697b73fa8c95b04215c889c``.
* ``wifi-key-contract-20261017T153417.711242Z``
  (SOURCE_AUDIT_NOT_DEVICE_PROOF),
  ``380cc780bb673851f5ce244b996b652bf8d4b6469848a9de2abdd3f4af1f1b58``.


Independent Wi-Fi FRW control codec in VM
-----------------------------------------

Source ``694dbd49a928e5111a2d3ca93f5d522b0e1959bd`` adds the independent
request encoder/reply parser at ``drivers/net/wireless/hisilicon/ws73/`` and
eight KUnit cases. The candidate control profile uses the pinned SDK110 Host
layout, explicit endian helpers and a 1024-byte local payload bound. It rejects
inconsistent headers/roles, tuple mismatches, trailing/truncated bytes and
oversize responses, while preserving outputs on error. Signed device status
is separate from parse success. Remote pointer slots are never dereferenced.
No SDK source, command executor, Wi-Fi adapter or RF capability is included.

A new dedicated, hardware-free x86-64 VM fixture uses the fixed
``build-wifi-kunit`` output under the shared build lock. All eight cases passed
with SMP, KASAN, UBSAN, lockdep and DEBUG_ATOMIC_SLEEP. Tests include all 40
short reply extents in exact-sized unaligned allocations, independent vectors,
NULL/size_t extremes, empty/maximum payloads, hostile address slots and signed
status extrema. The C files have strict checkpatch 0 errors/warnings/checks.

Final tested source ``2d2c861aa1527a4caef81866af1790b2ba3fed34`` strengthens
the runner's raw KTAP and termination validation; the production codec is
unchanged from the first run. The second full eight-case run also passed.
The runner now requires the exact ordered raw case list, totals, suite result,
reboot completion, JSON case set, requested config and successful process exit.
Both raw logs are complete and retained. Twenty-three corruption controls
reject missing/duplicate suites/cases, SKIP/failure, truncated/missing/duplicate
raw markers, wrong order, disabled KASAN and kernel diagnostics. This supervisor
exercise does not execute another device or confer protocol interoperability.
All evidence hashes from both runs were rechecked before this report.

The six-file Host source record additionally pins the request initialization,
FRW header and communication declarations, reply consumer and sequence lookup.
The original firmware producer's exact RSP flags/length behavior has not been
observed, so this restricted profile is a candidate until real U/E evidence.
Payload framing, active request correlation/sequence allocation, timeout/late
reply/epoch management, command policy, mac80211 integration and functional
STA are not implemented by these pure functions. Tests do not prove hardware,
firmware authenticity, security, regulator application or radio behavior.
No real USB was accessed. The pending NEW single-U power-cycle confirmation
and all previous firmware/E/peer/AP/upstream gates remain open.

Records and manifest SHA-256:

* ``frw-codec-source-20261017`` (source audit only),
  ``f49e6715b9f7e0c193c4045eb0c9c28b51850aaa86a2df3573c962fe01d6ca70``.
* ``wifi-frw-20261017T154421.224870Z`` (8/8 PASS),
  ``8ff27f1c0306eee17b0ec624284fa5c43b92d0b2be9435b9aae3bc8e686d256c``.
* ``wifi-frw-20261017T154725.316010Z`` (8/8 PASS, final supervisor),
  ``c906694b1763949f8bf999ebe9599d8d1e6f7c13e4a9a55fb8cd84476e4dd424``.
* ``wifi-frw-supervisor-20261017T154838.344250Z`` (23 corruptions rejected),
  ``3b62b75b42b2f48c2de4047909217cef5761edf67d639a94a0a9669c299c4336``.


Bounded Wi-Fi FRW transaction VM qualification (2026-10-18 SGT)
------------------------------------------------------------

Source ``6e7665a50f711dc7d2146c265aff4abb27bec68c`` adds the independent
transaction component and nine KUnit cases. It serializes admission, owns copied
reply bytes, requires both exact local send completion and a matching reply,
uses one bounded monotonic deadline and refuses sequence wrap. Errors preserve
the caller's result. Timeout, uncertain send, malformed/duplicate active reply
and abort are terminal. Device callback status remains separate from transport
success. Shutdown joins the admitted command; owner-side transport/RX draining
and device epoch validation are explicit external requirements.

Both the original eight codec cases and all nine transaction cases passed in
the hardware-free x86-64 VM with two vCPUs, KASAN, UBSAN, lockdep and
DEBUG_ATOMIC_SLEEP. Independent kthreads exercise reply arrival while send is
blocked and shutdown waiting until the admitted caller exits. These are actual
kernel scheduling tests using an injected FRW transport, not USB/hardware tests.
The final tested source ``dd594f03568c9c5cf7a97b822aa7a21fa2d4920d`` adds
the timely-reply/late-send-return boundary and consistent JSON totals; its
17-case VM run also passed. Strict C checkpatch reports 0 errors/warnings/checks.

The final supervisor requires both ordered suites/cases, consistent JSON totals,
raw KTAP summaries, VM reboot completion, requested configuration and tool exit.
Forty-six corruptions were rejected, covering missing/repeated suites/cases/raw
markers, SKIP/failure, truncated logs, reordered suites, nested/extra cases,
inconsistent totals, missing KASAN and kernel diagnostics. Manifest evidence
hashes for both VM attempts were rechecked. No additional physical USB access
occurred and no RF, original-firmware FRW response or functional STA claim is
made. The pending new single-U power-cycle confirmation, original firmware
startup, E hardware, peer/AP, security and upstream review gates remain open.

Records and manifest SHA-256:

* ``wifi-frw-20261017T160030.801885Z`` (17/17 PASS, initial source),
  ``bf02350be2b307f37f85f94eddb5d956e01e0ab5cd6212062aeefed37524f9bb``.
* ``wifi-frw-20261017T160203.151070Z`` (17/17 PASS, final tested source),
  ``2352b340f9652174ab02542a98600f19ae849e88c6b4c0b19bdb05e62afe2040``.
* ``wifi-frw-supervisor-20261017T160248.853248Z`` (46 corruptions rejected),
  ``8929c79e2b9159a30e8b7a841082b794abb03eb64217e4a52cdfd08d23334aba``.


Physical U command-echo ACK discovery and fix (2026-10-18 SGT)
------------------------------------------------------------

After a new user-confirmed single-device power cycle, the allocated U remained
at the same port/serial/topology and changed from USB device number 116 to 117.
Source ``87a946f9cabde2e2eac402451ca2f76dea245a7c`` passed empty-VM admission
preflight, then ran one actual device-level passthrough attempt. The first READM
before any writes completed with exactly four zero bytes; VERSION returned
``BOOTVER001`` plus two NUL bytes, and the loader's fresh READM again returned
four zero bytes. This adds one user-confirmed cold initial read on the same U
sample, not E qualification or independent power instrumentation.

The first fixed command ``WRITEM 4 0x40017750 51 `` completed OUT with 23 bytes.
Its IN completed successfully with 34 actual bytes. The first 32 captured bytes
were ``WRITEM OK:WRITEM 4 0x40017750 51``; usbmon text did not capture the last
two bytes. The old parser rejected the command echo with EPROTO (-71), leaving
``phase=3 stage=0 prep=0 image=0 ack=0``. The zero preparation counter records
accepted ACKs: it must not be interpreted as no hardware writes. Register effect
was not independently read back. No second WRITEM, FILES, payload or QUIT was
sent. Two callback drains, QMP removal, VM exit zero and no post-exit owner were
verified. The failure is retained, not relabeled as firmware-load success.

The pinned SDK110 Host compares only the success prefix. Independent fix
``5f4b7d964fb70a7f9e5a62c5546d7de7db339d21`` instead accepts only a bare
WRITEM ACK or colon plus the entire issued command, followed by zero padding.
It does not admit arbitrary suffixes or replay uncertain writes. All 85 KUnit
cases passed, including every echo truncation, mutation of every byte and
padding, maximum receive padding, and stop-before-second-write assertions.
The full 34-byte physical reply's ending remains to be verified in a new trial;
its successful return must not be inferred from the candidate test vector.

The FunctionFS loader fixture now returns 11 exact command echoes in one
successful original-file case, retains bare ACK cases, and injects a wrong
address echo that must fail before the second write. Full-speed and high-speed
runs each passed all ten scenarios with production USB, hashing and loaders,
original U/E files, no-extra-OUT checks and complete teardown. These remain
synthetic USB results, not physical firmware execution. Final executable source
is ``720bc66130370cd2105805b3d2b63e6185233d35``; it differs from the KUnit
source only in test formatting and supervisor checks, with production boot code
unchanged. Full-speed execution used ``6b5bdcfe6cf01f55ccc1c7afa2f441295f0a8b83``.

A new duplicate-echo corruption initially escaped the log supervisor when
appended outside all cases. That failure is archived. Global echo/bare-reply
counts fix the omission; 37 log and nine artifact corruptions are now rejected.
The tagged hardware trial additionally captures every successful boot RX byte
in bounded 32-byte chunks. Its private supervisor requires contiguous offsets,
exact full lengths, usbmon prefix agreement and the constrained ACK grammar;
22 synthetic trace corruptions were rejected. Final hardware build and empty
VM admission preflight passed. A new power cycle is required before the next
physical trial because the failed attempt submitted a RAM-control write.

Records and manifest SHA-256:

* ``power-cycle-confirmation-20261017T160819.051257Z``,
  ``c3e2d33575f540cb284495d94bb27afc2437fda1e596604d9376f65076e8478c``.
* ``hardware-build-20261017T160755.894777Z``,
  ``c63472698a03f5812ce41639a2f8cc92b73eb9164046f88a734073844604bad6``.
* ``ram-trial-20261017T160829.047812Z``,
  ``59027bfe2f6466b068df83423fa6d82ecc2dfd616aa979651f8e52efde4945e3``.
* ``ram-trial-20261017T160855.339264Z``,
  ``18b1bec4851def386afec995b6d4816af8554060680e0195bf4fea939650e0ad``.
* ``ram-trial-ack-analysis-20261017``,
  ``08ba17a0834ba09951d8893cb0b004a20352400a2b73edef525b413ae2b34099``.
* ``20261017T161238.336164Z``,
  ``3ad2a8c9faace3c31c7b4a7fe56105c54d5d8e0fc503eec270d404482ee0c41a``.
* ``virtual-usb-20261017T161642.578825Z``,
  ``609ace45f030df55cae837a0a92e732da38cf67ffdeee98c9d16c48b53f38a1f``.
* ``rom-echo-supervisor-first``,
  ``6459eb1cd2c64ebcc06638e68bc1abf4b52d6c7d66c3164acc346dff1c41a77d``.
* ``virtual-usb-negative-20261017T161751.032690Z``,
  ``22373efc7891342b1dfe23153c58cb8bee452d6fc7ce757830f2a5f91883e070``.
* ``virtual-usb-20261017T161826.290074Z``,
  ``95ba408da4cb82b48bad1662cde231adac5922806659202f1a0c58e2b4dd38ed``.
* ``ram-trial-verifier-20261017T161408.431817Z``,
  ``5726a9edbbd93eb1f7cd64fd1673e82154884805cc944da8af70e3dc2983c812``.
* ``hardware-build-20261017T161834.625174Z``,
  ``13af4f7c3fa2c0db4f3d9bc7c7c0cc694247d3cfe420f7587acd9652daf69fa5``.
* ``ram-trial-20261017T161850.906430Z``,
  ``36586e62bafe2712b9bbbf1bad62523e5bf452216c1be1f70358bbeb2af6b63f``.


U ROM READY echo and bounded shared TX admission (2026-10-18 SGT)
----------------------------------------------------------------

Signed change ``bf799e7664e91224652ca30ef9f938b5c3c48233`` bounds shared USB
TX mutex admission using the absolute deadline and a waitqueue. Contention
expiry cannot poison or modify an active sender's scratch buffer/sequence.
New tests hold one actual production send in its injected backend, check a
second caller's timeout, then check owner completion and later reuse. Separate
handoffs cover successful ownership transfer and propagation of the first
owner's transport error. All 87 KUnit cases passed under SMP x86-64 KASAN,
UBSAN, lockdep and DEBUG_ATOMIC_SLEEP. Full/high-speed native HCI synthetic USB
regressions each passed three scenarios (27 commands, 26 synthetic replies,
78 fragments, three drains). They do not establish radio operation or absence
of all concurrency defects.

The new user confirmation ``ACK 修复后已断电重插`` was recorded separately
from the previously consumed confirmation. The assigned U remained at 1-1.1.1
with the same serial/topology and changed node 117 to 118. One VM-only physical
trial used the clean bf799 source. Initial READM returned four zero bytes,
VERSION returned BOOTVER001 and two NULs, and the loader's fresh READM again
returned zero before any preparation writes. This is another user-confirmed
cold initial read on the same U sample, without independent power measurement.

All eleven fixed RAM/XO WRITEM transactions completed with full replies
``WRITEM OK:`` plus the exact issued command and one NUL. In particular, the
first 34-byte reply ends with space/NUL, now verified beyond usbmon's first
32 bytes. This closes the previous physical suffix uncertainty. Register
contents were not independently read back, and the explicit reference board
settings are still not universal hardware defaults.

The next command was ``FILES 1 0x400000 0x8000 0x0 ``. Its complete 42-byte
reply was ``READY:FILE01:FILES 1 0x400000 0x8000 0x0 `` followed by one NUL.
The old strict READY parser correctly stopped rather than accepting an unknown
suffix: ``result=-71 phase=3 stage=2 prep=11 image=0 ack=0``. Independent
transcript verification found exactly 15 OUT and 15 IN completions, including
13 completely captured trial replies. No firmware payload or QUIT was sent;
no runtime enumeration occurred. Two drains, VM exit zero and post-exit
unowned allocated device were verified. The physical trial remains FAIL, not
firmware-start success; this confirmation is consumed by this attempt.

Signed fix ``62d7821cf8567ec91b217d32f9ba4b9e114afaa4`` accepts bare READY
or the observed exact ``:FILE01:`` tag plus the entire current FILES command
and optional zero padding. It does not infer meaning for arbitrary FILE tags,
change FILES OK grammar or add retries. All 89 KUnit cases passed. The new
cases cover every truncated/padded length through 513, mutation at every byte
of a 512-byte receive, and rejection of a prior block's valid echo before the
next payload. Existing bare-token cases remain covered.

Full/high-speed original U/E-file FunctionFS loader runs each passed ten
scenarios with the production parser and USB path. The successful U fixture
supplies seven exact READY echoes across its blocks; other cases retain bare
READY replies. These are synthetic extensions of the one observed physical
first-block reply, not measured ROM behavior for all blocks or E. All 41 log
and nine artifact corruptions were rejected, including missing, duplicate,
wrong-length and wrong-tag READY evidence. The private complete-RX verifier
passed a synthetic transcript and rejected 24 corruptions. Its TX evidence
still combines full input hashes with usbmon's first 32 payload bytes, not
full wire capture.

The first loader command was rejected before VM creation because its CLI
omitted --firmware-root; the corrected invocation used the explicit SDK110
firmware directory. Hardware rebuild and empty VM admission preflight passed
at 62d782. A fresh allocated-U power cycle was requested for the next one-shot
trial because eleven preparation writes have now been submitted. The pending
trial must not reuse the consumed confirmation. E, board identity, runtime
firmware compatibility, real BLE peer/STA and upstream review remain open.

Records and manifest SHA-256:

* ``20261017T162748.206058Z``,
  ``f656af49cac008c1137b08b12206b8171430403607d484cae0f7a27ca1754a14``.
* ``virtual-usb-20261017T162915.228606Z``,
  ``0ee6e95aede76328470b770194a199bdf21e91cb4568da912d96e069a00a8335``.
* ``virtual-usb-20261017T162923.437139Z``,
  ``aa98d2a3bbbbe73755cb5e8f693998fd7f011990c3f191ffcc31655d7b8909ae``.
* ``power-cycle-confirmation-20261017T162930.462371Z``,
  ``a5fb1592936c62bff5a7284a26a477fd6a7a195c3578f295f426c4f8f733c77a``.
* ``hardware-build-20261017T162931.573520Z``,
  ``3cd9dac444a57faee63fc729a049343a127829e9a607b3f755ceb5499af50141``.
* ``ram-trial-20261017T162948.311091Z``,
  ``408d844b681c14ae098a8174a3bc211b8f52a3d978ccdca6afd0382ec6902e9b``.
* ``ram-trial-20261017T163233.162085Z``,
  ``86f6c0e43e5828cc9f107a9678b75e274477c20fea25f2d33a12c25260d7a585``.
* ``ram-trial-ready-analysis-20261017``,
  ``36e5380d0a9b9f1b86cb9174975f7ceb48cbb987b83c3bd89181361fa7f34cde``.
* ``20261017T163501.143241Z``,
  ``5f7a8f33ad20d77f53fda10038547b5a03894ac41e35b723a0d397e85ca9ec63``.
* ``ram-trial-verifier-20261017T163450.630580Z``,
  ``43687c1cb919692ca970e3bb716aa888315a07af592811198eeeb2ecddadc9c7``.
* ``virtual-usb-build-20261017T163547.830571Z``,
  ``24dfb09c354e31a46043a7f18fd70452f7449ed452f2c33e77401724dfc72715``.
* ``virtual-usb-20261017T163629.672957Z``,
  ``19fa40ea3f8871d9aef1dc603b3fd47a061f3fcffabcadcd442f1552789b3e9d``.
* ``virtual-usb-20261017T163639.704156Z``,
  ``1e84fb0f62a87bfe301a8346d13ab3ad701db470c7866929f5091c045488841d``.
* ``virtual-usb-negative-20261017T163654.495024Z``,
  ``5df23438e421c2d92fb8fd52a970b73edc4dbd8f7792ce08da8608460493117a``.
* ``hardware-build-20261017T163647.842799Z``,
  ``e27c427393288c3405324c9aff4cd28db1450f2fcd67c61fc858240a0cb1570f``.
* ``ram-trial-20261017T163704.118738Z``,
  ``cde216711081143d7ebf1ee736c1a55e81d474005e72d858d00d8a047a7e4b7a``.


Post-ACK and TX-admission cross builds (2026-10-18 SGT)
-----------------------------------------------------

Clean source ``fa3cd51e7d47029c50f083ea01dc9a6beb2f487c`` was compiled and
linked after the READY echo and shared TX lock-admission changes. The four
sequential profiles were arm64 Clang, i386 Clang, PowerPC64 big-endian Clang,
and x86-64 GCC. Each used W=1, four jobs, the dedicated fixed incremental
output directory and the shared build lock. All four passed; the retained
build logs contain no warnings or errors. A second audit verified every
recorded artifact hash and all seven ELF headers per profile (vmlinux plus
six modules), including class, endianness, machine and object type. Requested
shared/USB/native Bluetooth modules and LE-enabled, BR/EDR-disabled Kconfig
settings were checked against the resolved configurations.

This refreshes the compile/link evidence for these production changes. It
neither runs foreign-architecture code nor qualifies Wi-Fi, hardware, all
Kconfig combinations, PM or recovery. No physical device was accessed; the
new READY-fix power-cycle confirmation is still pending. The source remains
compatible with the already prepared hardware build because changes since
62d782 are documentation-only.

Records and manifest SHA-256:

* ``cross-arm64-clang-20261017T164144.670215Z``,
  ``be0f091d7b46cb9f756aa2427ee37630aea2c2ac1a38b6b51bc0e7b3ff77c321``.
* ``cross-i386-clang-20261017T164158.140355Z``,
  ``7d29561241de1dc32f8e3ef09b387281bf7e029754d0563477166914c42cc860``.
* ``cross-ppc64be-clang-20261017T164216.660388Z``,
  ``f8c599fc83b698d7b936d042ab12d35f49b08537a403f7c71f2ba73a31049b97``.
* ``cross-x86_64-gcc-20261017T164231.479704Z``,
  ``01cc2709757c43f4f77fefe711daac05473c7b22cb13c844563e3b3aec6281ee``.

Independent audit ``cross-current-audit-20261017T1643.json``, SHA-256
``4b166d19ca07f4fb2097a9bb7e8a97d92866e98295dd75a4fc17ce7d536c9231``.


Archived KCSAN startup report: exact field mapping (2026-10-18 SGT)
-----------------------------------------------------------------

The existing failed diagnostic VM ``20261017T144617.720728Z`` was analyzed
without another VM run or instrumentation change. Its original vmlinux,
config and log hashes were rechecked. Disassembly resolves the reported
``kthread_is_per_cpu+0x1b / worker_thread+0x5c`` accesses to the same four-byte
``task_struct.flags`` at offset 0x2c in this binary, not to the flags in the
separate struct kthread or to a WS73 object:

* The reader's return address 0xffffffff8130fc2b follows the read4 check at
  0xffffffff8130fc26 on task+0x2c. The following test of byte task+0x2e mask
  0x20 corresponds to the inlined tsk_is_kthread PF_KTHREAD (0x00200000) test.
* The writer's return address 0xffffffff8130707c follows the read_write4 check
  at 0xffffffff81307077. Surrounding instructions read task+0x2c, OR 0x20
  and write it back under wq_pool_attach_mutex: inlined set_pf_worker(true)
  setting PF_WQ_WORKER. The reader does not take that mutex.

kernel/kthread.c, kernel/workqueue.c, include/linux/kthread.h and
include/linux/sched.h were byte-compared across the original diagnostic HEAD,
current review HEAD 68ec2e603404 and base 70dd4ee72eb2; all are unchanged.
The line resolver could not provide exact source lines from this artifact;
the mapping instead uses the captured offsets, instructions and source.
The report occurred before KTAP and contains no WS73 stack frame. This
narrows its accessed object and operations, but is not a separate no-WS73
baseline reproduction or proof of complete causal independence. The original
VM also had temporary fair-scheduler instrumentation, whose timing effects
remain a limit. No kernel fix, KCSAN suppression or external report is added.

The startup access is now located, not fixed or classified as harmless.
Overall KCSAN qualification and the separate release_task/blake2s unknown-writer
report remain unresolved. No driver success is inferred from this analysis.
Record ``kcsan-worker-field-analysis-20261017/manifest.json``, SHA-256
``1a2f004db3ea4760edc06a30303370b3340d07fec5a559f6bb912344e01f32b2``.


Physical original-file download and runtime reply boundary (2026-10-18 SGT)
-------------------------------------------------------------------------

The user-confirmed READY-fix power cycle (device 118 to 119) was consumed by
``ram-trial-20261017T170231.747329Z``, built at 62d7821cf856. Eleven preparation
replies and the 42-byte tagged READY passed. The first 32768-byte code transfer
completed successfully, 6858 us after READY. The ROM returned the complete
38-byte ``FILES OK:FILES 1 0x400000 0x8000 0x0 `` plus NUL. Strict token-only
parsing rejected it with -EPROTO, phase 3/stage 4/prep 11/image 0/ack 0.
No next block or QUIT was sent. Zero acknowledged bytes does not mean zero
transmitted bytes. Both owners drained and QEMU exited; original status FAIL
is retained. Independent analysis checks all 16 OUT/16 IN transfers, 14 full
trial replies, original input hashes and the consumed confirmation.

Signed fix cd3d210661019f09b3f32b9268510a6834f5d771 accepts only the exact
current FILES command after ``FILES OK:`` or the existing bare token, with
optional NUL padding. KUnit covers every reply length 0..513, every byte
mutation in 512 bytes and a stale first-block acknowledgement at the second
block. All 91 KUnit cases passed under KASAN/UBSAN. Full/high-speed original
U/E USB loader fixtures each passed ten cases, including seven FILES echoes
and fifteen bare FILES replies over 22 payloads. Supervisor controls rejected
45 log mutations and nine artifact corruptions. The private full-boot trace
verifier rejected 26 corruptions. These are offline/VM results, not E hardware.

The next user confirmation, explicitly without other supply (device 119 to
120), was consumed by ``ram-trial-20261017T171116.336919Z`` at cd3d21066101.
All eleven preparation writes, seven READY/payload/FILES OK exchanges and QUIT
passed: original payloads 143892, 20980 and 7708 bytes, total 172580. Independent
analysis checks all 29 OUT/28 IN completions, 26 full replies and each READY
holdoff (6733..10819 us). TX capture remains a 32-byte prefix plus original
input hashes, not a full payload wire comparison. The loader reached
phase 4/stage 6/image 2/ack 7708, which means waiting for runtime, not Ready.

The VM did not observe runtime within 30 seconds and the overall trial remains
FAIL. Cleanup drained both owners and QEMU exited zero. After cleanup the
same serial/topology exposed device 121, 480 Mbps and the five-endpoint runtime
descriptors. This post-cleanup observation cannot prove when or why the switch
occurred inside the original VM. The QEMU log retained libusb NO_DEVICE at
release; the guest also logged a clocksource watchdog CPU-read timeout. Neither
is suppressed or asserted to explain the transition. No automatic reload or
alternate firmware was attempted.

A separate VM, ``runtime-observe-20261017T171428.464520Z``, attached that runtime
without firmware files or a loader module. It received BSP_READY, completed
two 36-byte USB controls and a 128-byte query, then received 188 bytes into a
98396-byte RX buffer. The captured outer prefix is type 2, length 188, queue 9,
first record length 96. The driver returned -EMSGSIZE while processing the
chip observation and did not query version. Only the first 32 bytes of this
reply were retained, so its body and exact failing field are not established.
One owner drained; QEMU exited zero and the device was unowned. This is a real
runtime response, not verified chip identity, firmware version or BLE support.
The original FAIL is retained; the independent partial-transcript audit does
not promote it to PASS.

Signed test-only change 44c5204b441fef293037bef4f3639a79e4ebdc66 adds binary
usbmon recording inside the isolated guest: a 1 MiB ring, at most 512 copied
bytes per event, explicit actual/captured/copied lengths, and drop detection.
It leaves the production parser unchanged. Both guest recorders compile with
-Wall/-Wextra/-Werror. The refreshed hardware build and empty VM preflight
passed, including binary usbmon open and the eight existing admission guards.
The private runtime verifier was corrected to the actual 98396-byte production
RX capacity; its synthetic positive and fifteen corruption controls passed.

The next prepared trial may explicitly detach the completed boot instance and
reattach the same independently checked runtime device through QMP, preserving
separate owners and drains. It must be labelled controlled handoff, not natural
reenumeration or recovery. A new power cycle is required before another loader
trial. The two confirmations above are consumed. Normal autoloading remains
observation-only; U/E identity agreement, board applicability, E/peer/AP,
BLE/STA and formal upstream gates remain open.

Records and manifest SHA-256 (under the private evidence root):

* ``power-cycle-confirmation-20261017T170217.602729Z``,
  ``b81f66f196449faa42ba43a083ca77e7b54c562a5d89e5c79b290c692411e3d6``.
* ``ram-trial-20261017T170231.747329Z``,
  ``17b5d5a6e36ba3969084b075695afa8d87e55a6b2b04e2127ade4266d911b4e7``.
* ``ram-trial-files-analysis-20261017``,
  ``057ed9d071f4e2c582a316097d4ad35f588d0fb15e823c9cf3cb70ef4c0754ec``.
* ``20261017T170731.111495Z``,
  ``cecf4a2cdd6c8f9b119052397e993b8c150f286c71619620424f0f2f1ba7040c``.
* ``virtual-usb-build-20261017T170817.352209Z``,
  ``2080692469c344fad8ea72ad3d76087d36f56fdaf7fd17cabb22f70da4fcc753``.
* ``virtual-usb-20261017T170847.795612Z``,
  ``2170be16a291245bf57860fbd977f970861ddd96aa56f7f91adf4b676debea8e``.
* ``virtual-usb-20261017T170905.532591Z``,
  ``b27f54eeaf429fad784f44fef2e55d724c61fa1621d51753f0aae00d13dd9893``.
* ``virtual-usb-negative-20261017T170906.626172Z``,
  ``44cf96fd36166f54c2b7fad9d733cd30b4db5a2884835463ca2f342d97225a0f``.
* ``hardware-build-20261017T170928.503885Z``,
  ``164d52a13f579576ee5bdc189acf8626c05f0c7711d0bf1da624d71137f46a97``.
* ``ram-trial-20261017T170955.502519Z``,
  ``c323e46cb9c16336680249d0ad3a24fec8b464968af685bdee82b99cee112fe3``.
* ``power-cycle-confirmation-20261017T171103.403865Z``,
  ``7ea3418b8cb56d3174704150a27dc9b761feaa58d0dfd4c440f1704eff889356``.
* ``ram-trial-20261017T171116.336919Z``,
  ``93d49547d292bf424550750b0d60b607d6f1323d1894e9524d1d31e235190933``.
* ``ram-trial-load-analysis-20261017``,
  ``4dbac3262ac45c5b5726af792ac455bf47f83a3596723f02875d142221b3e7b2``.
* ``runtime-verifier-20261017T171410.615236Z``,
  ``e754a3e679091c95140f9cf5b813ca50f497107a178ddab71ae56573c816fac4``.
* ``runtime-observe-20261017T171413.165356Z``,
  ``d7b18a9652a8782d76bbc71eec89ea3cac3c357067b1d2c4469e30fb394517a8``.
* ``runtime-observe-20261017T171428.464520Z``,
  ``c562894257c35b60bdc2517dcee26530809c05d3c49c2c858e273a3eb1cf47e9``.
* ``runtime-size-analysis-20261017``,
  ``b557f81d612b4de0642887fa7fb476357f0f8bcc28f1363bb7bbbf13e2ca6c96``.
* ``runtime-verifier-20261017T171641.368691Z``,
  ``37dbe2cc3b9e8a17025adef4fb495c3eb9eb576f42e7142aec76bd2c6ea6b096``.
* ``ram-trial-verifier-20261017T171748.195818Z``,
  ``22a5621618edcb7762b9ab26312ca84022b25e3c4ddd7e9cc6e974e631dde44f``.
* ``hardware-build-20261017T171747.137702Z``,
  ``4c2f8c5a4582a44db50e91716f40df77fde721b3c7ea37e0f5813eff59930646``.
* ``ram-trial-20261017T171833.202747Z``,
  ``2a3624c37a51fe8453fe20c9978dce33ce50de9c188c7e4e76c21d6b1127e78c``.

Platform allocation and complete physical runtime observations (2026-10-18 SGT)
-----------------------------------------------------------------------------

Signed change 6abc104ac5beed960b5b71a5a3680f1b26da2cbd separates platform
allocation length from the embedded returned-data length. The SDK110 Host
callback copies the returned length at data+4, with data beginning at +8;
it does not require the allocation to equal the populated fields. Independent
inspection of four existing, hash-bound other-project captures (one E, three
U) found 188-byte replies, 96-byte HCC records, allocation 88 and returned
length two. They are protocol references, not this project's hardware tests.
The production codec now requires allocation 10..record_length-8 for chip,
16..record_length-8 for version, with exact returned lengths two/eight and
unchanged status, ID and outer bounds. Unused bytes are not interpreted or
published; no SDK code or captured allocation padding was imported.

Clean-source validation at 6abc104ac5be passed 92 KUnit cases under KASAN/UBSAN.
The added test checks every u16 allocation length, truncated input, nonzero
padding and unchanged output on error. CBMC passed the bounded i386/x86-64
converter contracts and five mutation counterexamples; this remains a pure
converter proof, not a concurrent transport proof. An initial dirty-source
KUnit run is retained separately and excluded from clean qualification.
The first USB fixture build failed -Werror=sign-compare before starting a VM;
test-only d4844eaa58bce852d2b7f792692c09990fa0369d corrected the two write
comparisons. Full/high-speed runtime matrices then passed all seventeen cases
each, including an independent 188-byte chip/version fixture with nonzero
allocation padding. Supervisor checks rejected 32 log and five artifact
corruptions. No physical support is inferred from those fixtures.

The user's "complete capture" power-cycle confirmation was bound to boot
host device 122 and consumed by ram-trial-20261017T173157.445964Z at d4844eaa58bc.
Before writes, fixed READM again returned U/USB. All three original inputs
passed validation; eleven preparation writes and seven payload blocks sent
172580 bytes, followed by QUIT. This trial explicitly used QMP detach and
reattach after checking the same physical runtime identity/descriptors. Boot
and runtime had separate owners (guest devices 2 and 3, ports 1-1 and 1-2).
This is a controlled handoff, not natural reenumeration or recovery.

The real driver then completed both runtime requests. Full binary replies
were 188 bytes each, with 96-byte HCC records and 88-byte platform allocations.
Chip returned package 0 / bus 0, agreeing with the pre-load U/USB reading;
version returned firmware 0x010a6e3c and ROM 0x00010101. The independent audit
verifies the complete two query bodies, both returned data fields and all
boot/runtime transaction ordering through the archived strict validator.
All 128 binary bulk events pair and drain; four binary statistics reports
show queued=0/dropped=0. Three owners drained, QEMU exited zero and the checked
device was unowned. The clocksource watchdog CPU-read timeout remains logged
and unexplained.

The original supervisor result remains FAIL: text usbmon contains only 115
bulk events and first reports an undrained URB. Independent comparison finds
thirteen contiguous missing text events from the Wi-Fi-calibration completion
through QUIT, not just one missing completion. Every retained text event is
an ordered, matching subset of the binary stream. No missing event was
invented and neither the archived log nor original manifest was rewritten.
The separate analysis is labelled SCOPED_BINARY_HARDWARE_ANALYSIS_PASS with
ORIGINAL_TEXT_GATE_FAIL; it establishes the transaction scope above only.
Large TX payloads remain prefix/hash/actual-length checked, not full wire
byte comparisons. Firmware authentication, general board applicability,
BLE/HCI or Wi-Fi RF behavior and E qualification are not established.

Test-only feebabc81807 bounds each binary drain to sixteen events (including
skipped control events), allowing text/kernel readers to run between batches.
It encodes captured bytes before one printf rather than issuing a printf per
byte. The kernel text recorder has a separate finite event queue; delayed
reading is a plausible loss mechanism, not a proved cause of this capture.
The original strict text/binary correspondence gate is retained. A mock ioctl
harness checks batch bounds, 512-byte exact output, skipped traffic, EINTR,
EAGAIN, EIO and dropped-event failure with GCC and Clang; Clang ASAN/UBSAN also
passed. GCC sanitizer linking lacked system runtime libraries; the first
sandboxed Clang run failed LeakSanitizer's ptrace restriction. Those attempts
are not passes. The final user-space sanitizer run outside that restriction
passed without suppressions. The refreshed build and empty VM preflight
passed; no new physical run has qualified the recorder change.

Records and manifest SHA-256 (private evidence root; raw padding stays private):

* ``platform-allocation-audit-20261017``,
  ``891de46112e11a88f2262c38470b217213bf318bb2afb6b7ace2d2f19a6f2450``.
* ``20261017T172720.313072Z``,
  ``44cbd16ba218084936dfbfa67b56670946cfc3ab7d351bc1dd50633d1d9933d6``.
* ``cbmc-hcc-20261017T172756.105106Z``,
  ``f6d6060f3a19035a553fa33d9bffb94f85e414088b201ad41925ffd40cb6f7ea``.
* ``virtual-usb-20261017T172941.997790Z``,
  ``41dc60481e7f59293015a6f8384f4e2f916384ed6bc877f2abe7bb17a6531e9f``.
* ``virtual-usb-20261017T173019.914957Z``,
  ``b49f0b8e014dbad3396b119d8038b168158d3c459261a94875d6f57c4d972af6``.
* ``virtual-usb-negative-20261017T173020.952339Z``,
  ``f67a70d6a5efee7bf8eed6e4d167fbf1953edf48b4e8fea30b0b98f5f2a3344f``.
* ``power-cycle-confirmation-20261017T172934.246860Z``,
  ``afb3f22f9b53bc3075b3bc6938f190cc4a36b3caa37c6b80bfddd2ba89db9997``.
* ``ram-trial-20261017T173157.445964Z``,
  ``2c22911ade350bed9cebdb36bf773e7c433aafd7d67836971efdec22df6a759a``.
* ``ram-trial-runtime-analysis-20261017``,
  ``bffa10f9cdaedaee81a7a43d6eac7ade18a91bdde5533f58679b0924e3a75690``.
* ``usbmon-fairness-20261017T173838.612666Z``,
  ``37081b4688b7e6e81450eb103aa73bccd5e611870ba52432f00fd3f0721d3b7c``.
* ``hardware-build-20261017T173852.997819Z``,
  ``79df1465d543ba0504b421008fa2af35df05f2ac044160ccfa0b18f917f4d2a2``.
* ``ram-trial-20261017T173923.371584Z``,
  ``ef342d3b12022fd91209f5229aee087253fecbc0ab65d80326345c0f23dea612``.

Platform PM prerequisite in BLE startup (2026-10-18 SGT)
-------------------------------------------------------

The next physical BLE preparation audit found a missing shared startup step:
``ws73_board_service_enter`` sends platform PM configuration before the BSLE
customization/enable path. Signed daaee4b511ccd9d5a6bdb176c064d0721a55bf38
adds its independent encoder and bounded send, and makes it mandatory between
BSP and customization. Request 0 carries four PM bytes in a 32-byte HCC
record. Wake GPIO, active level and USB-link checking are explicit caller
inputs. Sleep and reserved bits stay zero. The sleep policy deliberately
differs from the SDK default INI's enabled sleep because Linux device PM is
not qualified. Success reports local transfer only, with no invented PM ACK.
Short/error/cancelled TX prevents customization and enable; the one-shot
startup still shares the original absolute 12-second budget.

Source audit also distinguished the UART-only platform eFuse message path
from the USB register path. SDK address initialization can fall back to a
random base and derive BLE from it; get_dev_addr or READ_BD_ADDR success alone
does not establish a provisioned public address. No eFuse operations, address
fallback or public HCI setup bypass were added. Temperature compensation is
off in the pinned INI. Default/light Host configurations disable heartbeat,
whereas USB-light omits its symbol; its conditional Host no-op implementation
is not evidence that every firmware has no heartbeat requirement.

A private 140-byte reference customization candidate was prepared from the
pinned INI with exporter provenance. Its PM candidate keeps GPIO10/high but
clears sleep (0x215 -> 0x214), with USB-link checking off. These inputs were
not sent to any device and remain NOT_BOARD_QUALIFIED. No calibration blob,
SDK code or private raw capture was added to the public branch. Physical
publication stays gated on the remaining platform/address prerequisites.

Clean daaee4b511cc passed 94 core KUnit cases and the separate 31-case native
HCI configuration with KASAN/UBSAN. Tests cover output preservation/capacity,
independent PM byte vectors, range rejection before I/O, each short-transfer
stage, cancellation/deadline, terminal transport state, startup ordering and
failure preventing action. The first core attempt built but QEMU could not
bind its QMP socket under the sandbox: zero KTAP tests ran and FAIL remains.
The independent permitted KVM run passed without weakening the checks.

At 0eee3af3937b, full/high-speed USB startup each passed seven cases and
52 bulk OUT/30 bulk IN; native HCI each passed three cases and 57 bulk OUT,
101 bulk IN, 27 HCI commands/26 synthetic replies and three drains. Both
positive HCI instances completed standard management-info reads and auto-off;
Reset cancellation retained the one-second drain requirement. Firmware and
HCI replies in these runs are solely the explicit FunctionFS fixture, not
physical WS73 behavior. The production parent remains observation-only.
Startup and HCI supervisors rejected 29/34 log mutations respectively and
five artifact mutations each, including missing/short/reordered PM records.
The current all-module Clang W=1 build passed, including cross-module startup
and HCI symbol linkage.

Two verification-tool failures are retained: after adding platform-config.o
to archived build artifacts, the USB consumer's exact required set initially
omitted it and rejected before VM launch (fixed by 0eee3af3937b). The first
HCI negative-control run then refused its own unchanged wrong-opcode mutant:
PM TX moves the first HCI command from sequence 9 to 10. 01b06c8b743e updates
both sequence/opcode mutations and verifies all 34 controls. Neither failure
was converted into a pass or removed. 5edc6d449eca only adds artifact retention,
ordering assertions and source formatting relative to the tested core change.
The kernel delta has zero checkpatch errors and the new-file MAINTAINERS
advisory; development fixture formatting still follows its existing style.

There was no new physical trial in this increment. Previous allocated-U
binary runtime identity/version evidence remains bound to its own source;
it does not qualify the new PM path, BLE actions, addresses, E, a peer or RF.
The original U/E joint submission and external-review gates remain open.

Records and manifest SHA-256 (private evidence root):

* ``platform-startup-audit-20261017``,
  ``cc34a926aa5a7a3ad0974cb6eddc1628434bbc0676109d71da00ff515a03ab37``.
* ``ble-reference-input-20261017``,
  ``7b3facb8ad73c70644a44dd3e1e4038eaf89a89b2f97c0ced6873e0bdde97317``.
* ``20261017T174848.118693Z``,
  ``5d2ea5e21528c8fa09fb4decc6945a2b5d945239c23f469e01459dd70646a07a``.
* ``20261017T175000.305750Z``,
  ``8020cab43d87afebc18f97d887f0fd2f489a98e1121d73cd86e63f2f47ce16b3``.
* ``20261017T175049.275788Z``,
  ``a32824129ff9f94df21d36e4218bd94b8b10780963903ff3c8013fa7a94cedff``.
* ``platform-artifact-rejection-20261017``,
  ``5d2c12f3a829cd4446e2940a981d774670de56f9a46174189a88fc91db58f484``.
* ``virtual-usb-build-20261017T175533.063208Z``,
  ``1188c0801afa30cf6977109f4067b106dcc435e028cd6409036024bbce6898c0``.
* ``virtual-usb-20261017T175555.026494Z``,
  ``e7f8543f307d3b4fc13ee715a2eaf7e254c76665b3ccd1703ec9702fed133d26``.
* ``virtual-usb-20261017T175619.303652Z``,
  ``d0aba3c43aef42b2c5c007aa488238b57c9e3544b7b725b096f4db1d808f306b``.
* ``virtual-usb-negative-20261017T175617.124563Z``,
  ``daac2cee27ea0b9517c2f6fa1140afa84f4908b4ef6911316320774b576074ee``.
* ``virtual-usb-20261017T175651.575217Z``,
  ``f319805adfdb332b2b7a19b9eb676792702c6f5e3b7e0125240ed39f2e43bad8``.
* ``virtual-usb-20261017T175715.066900Z``,
  ``5dbe26153a373faf7a77a03b4657c50f5670479d56b388e44c3ac25aa06bbab8``.
* ``platform-hci-negative-rejection-20261017``,
  ``743e4f49befbfdfd818f9d0be47ea6681a99885c1d6c7014de07e0206877abec``.
* ``virtual-usb-negative-20261017T175744.542427Z``,
  ``e730d73e0600ffdcc974dd49f02edf131d6b4bf68e73071da5482abaa154f248``.
* ``hci-build-module-20261017T175749.474079Z``,
  ``592dd2b5301090b64069ef789dd2f3ceb23ad62931a3b8145cae1609684d1c1d``.

Allocated-U BLE trial admission and retained-runtime timeout
---------------------------------------------------------

Source ``923f77adde928e8270a57c1720f4ac92d1f086fd`` adds a tagged-VM,
explicit-PID-1-bound BLE startup test module.  It validates an exact private
140-byte reference customization input before USB driver registration.  No
reference payload is included in the public source.  The worker arms readiness
before RX, requires U/USB plus the previously observed SDK110/ROM versions,
then runs the production awake-platform/customization/enable/BOOT sequence and
one explicit disable.  It never publishes HCI or sends radio activity commands.
The regular USB parent remains observation-only and board/address gates remain.

The Clang W=1 hardware build passed.  Empty-VM preflight passed eight admission
checks, 141 rejected profile sizes, 140 one-bit corruptions, two null cases and
one accepted profile, plus missing/invalid enumeration module rejection and
empty registration/unload.  These are offline/VM properties, not BLE support.

A single physical attempt on the allocated U reused the exact same unowned
runtime host enumeration as the preceding RAM-load record.  Guest explicit
binding succeeded, but the first 36-byte queue-9 switch on OUT endpoint 1 timed
out.  The driver returned ``-ETIMEDOUT`` at ``identity``; USB cancellation
completed with ``-ENOENT`` and actual length 36.  That length does not prove
firmware acceptance.  There was no identity/version reply, PM configuration,
customization, BLE class request, HCI publication or firmware reload in this
attempt.  The cause remains unresolved; neither sleep nor an endpoint bug is
established.  Endpoint 1 matches SDK descriptor index 1 and the preceding
successful physical transcript; endpoints at indices 3/4 are register access.

The original supervisor result remains FAIL.  An independent failure audit
checks all archived hashes, four device bulk events and two interrupt events,
all three paired cancellations, zero successful RX, one owner drain and VM
exit 0.  Both capture streams agree and every binary drop counter is zero.
No KASAN/UBSAN/lock warning was observed.  Host ownership was released, but USB
cancellation is not evidence that firmware state has been cleared.

A private continuous load-to-BLE trial has been prepared and passed an empty
VM preflight.  It retains the previously checked boot READM/VERSION and
three-file loading path, disables automatic authorization of future guest
interfaces before loading, then explicitly binds a new runtime owner after
controlled QMP handoff.  A new physical power-cycle confirmation is required
and consumed once before attachment.  This is pending hardware execution, not
natural reenumeration or BLE qualification.  Independent transcript checks
reject 12 BLE and five boot corruptions; synthetic positives establish checker
behavior only.  The initial BLE checker assumptions were corrected offline:
ignore virtual root-hub IRQs, use endpoint 1, and compare ordering using text
usbmon timestamps rather than mixing text and binary clock domains.  Original
failed-trial tools remain archived unchanged.

The 512-byte binary capture retains complete expected RX and the entire
140-byte customization body.  It does not retain the last 352 padding bytes of
an 864-byte customization TX; a future success must retain that scope limit.
No E, address provisioning, peer, RF, STA or formal-submission gate is closed.

Private records and manifest SHA-256:

* ``hardware-build-20261017T180514.506244Z``,
  ``c47e8b9cd17ec360abcba4da878f07ddc3e867caad7e615ce3405f634e5f5e4b``.
* ``ble-startup-trial-20261017T181109.269163Z``,
  ``b0c87d8aae199f575408928b297f548e0aa13403fd41e27b3c8a47540289b893``.
* ``ble-startup-trial-20261017T181210.020136Z``,
  ``41732fff8d13774cd594ecb4d267582a0a6656634375c5302e03378dac62a12d``.
* ``ble-startup-failure-analysis-20261017``,
  ``fd604cd6d1ea0016813b8a739d293f31c30da73c1791b7f6f676b55a1aec8498``.
* ``load-ble-trial-20261017T181504.963546Z``,
  ``5b602a7248de426d9347b3fbf07dce31c759616606fdff84fc921177074b96db``.


Independent Wi-Fi RX storage bounds (2026-10-18 SGT)
--------------------------------------------------

Source ``efe8529eae50`` implements the inner receive storage boundary already
identified by the pinned SDK audit: 32-byte control block, 36-byte reserved
header region and body. The new parser validates actual buffer capacity before
returning immutable header/body spans. It never uses the device header address,
moves data, changes skb length, strips crypto bytes or creates mac80211 flags.
Raw channel/RSSI/cipher/status remain unqualified metadata. A future consumer
must validate HCC/FRW, VAP/epoch, 802.11 header and the firmware-specific meaning
of these fields before delivery. No adapter or transport consumer is added.

The first candidate subset is one buffer and non-AMSDU, with local 4096-byte
allocation and at most 31 trailing padding bounds. These are explicit initial
policy restrictions, not measured firmware limits or supported MTU claims.
The actual device producer, security metadata and TX completion dependencies
remain open. This increment does not change the approved mac80211 route.

The hardware-free x86-64 VM passed 22 KUnit cases: eight FRW codec, nine
transaction and five new RX cases, with KASAN, UBSAN, lockdep and
DEBUG_ATOMIC_SLEEP. The RX cases cover exact-sized unaligned truncations,
all six-bit header lengths, frame/header underflow, actual-body overflow,
all accepted padding sizes and the rejected next size, unsupported buffer
counts/AMSDU, valid input after rejection, maximum allocation, size_t extremes
and NULL. A hostile device pointer and poisoned unused header reservation are
not exposed as frame bytes. Rejected input preserves the output object.

The runner now requires all three exact suites, ordered raw cases and JSON
counts, clean VM termination and the two new compiled objects. Its independent
supervisor check rejects 61 missing/duplicate/SKIP/failed/reordered cases,
truncated logs, inconsistent totals, disabled sanitizer configuration and
kernel diagnostics; RX suite-specific mutations are included. Every mutation
is checked to differ from the positive. These checks verify the supervisor,
not original-firmware compatibility.

The current-source hardware trial build and its empty-VM continuous-load
preflight also passed. No physical USB was accessed during this increment;
the new user power-cycle confirmation remains pending. The preceding retained
runtime trial remains FAIL. Earlier U boot/load evidence, E/peer/AP gates,
address provenance and formal upstream submission status are unchanged.

Private records and manifest SHA-256:

* ``wifi-frw-20261017T182319.566522Z``,
  ``6f9ae1e9a29f128d05f000adf133abf9ac940b53a7c93ba70721d959b3228c41``.
* ``wifi-frw-supervisor-20261017T182412.090053Z``,
  ``a70d2ee82d98e66225af0d845d7c5c970771aa7d66abed407ddfa249f431fb37``.
* ``hardware-build-20261017T182412.153454Z``,
  ``4e8df5f318b9345478b73f7901b5c55d1951c16091cf9b7c8b39361b17c5320c``.
* ``load-ble-trial-20261017T182455.149845Z``,
  ``45999f1e20ef1221492117d705453e03e6bad6a880cf67750c2df10b14f30eb8``.

Allocated-U continuous RAM load and BLE enable/disable (2026-10-18 SGT)
--------------------------------------------------------------------

Source ``f88f2a4e105298be809793bb9e9b5ff75ea449c0`` passed one physical
trial using the assigned U in the isolated, two-vCPU VM. After the user's
power-removal/reinsertion confirmation, host enumeration was initially absent;
read-only checks did not consume the confirmation or start a hardware trial.
The same serial reappeared on the assigned port at 06:11:05 SGT following
another reported reinsertion. Its exact boot descriptors and lack of ownership
were checked before attachment. No hub/controller recovery was performed;
the cause of the earlier missing enumeration remains unknown.

The production boot path read HCC identity before selecting the three original
SDK110 U files, validated all inputs, performed the 11 fixed preparation
operations and downloaded 172580 payload bytes in seven blocks. The transcript
contains 29 boot OUT transfers and 28 replies, including the initial observer
queries, fresh loader identity, READY/FILES OK exchanges and QUIT. The result
reached WAIT_RUNTIME. The explicit reference XO/calibration choices retain
their earlier board-qualification limitations.

Controlled QMP detach/reattach created a new runtime owner on the same guest
port with a new enumeration number. U/USB and firmware ``0x010a6e3c`` /
ROM ``0x00010101`` matched. The production startup path received BSP readiness,
sent the awake-platform policy and reference customization, received the
customization/action/BOOT notifications, then completed one explicit BLE
disable with its device reply. The verifier checked eight runtime bulk OUT,
six successful bulk IN, four function notifications, one successful interrupt
and two completed class requests (enable 25, disable 26). Neither a host write
completion alone nor a synthesized HCI event was used as a success criterion.

All archived material hashes match. Text and binary bulk captures agree,
binary drop counters are zero, and complete runtime replies are retained.
The 512-byte customization TX prefix includes all 140 input bytes but omits
the final 352 padding bytes; this is not a complete-wire-payload capture claim.
Three device owners drained, QEMU exited 0, and the target returned unowned
at runtime host enumeration 125. No KASAN/UBSAN/lock warning was observed.
Read-only postchecks confirmed unchanged serials/enumeration numbers for all
four SparkLink devices; none was attached, reset or otherwise operated on.

This is real U initialization/enable/disable evidence, not native HCI, RF,
peer communication, address provisioning, E support, repeated-start endurance
or natural reenumeration/recovery qualification. The earlier retained-runtime
timeout remains FAIL with an unresolved cause; this fresh-load success does
not establish a repair or justify automatic retry. The production parent
remains observation-only, and the joint U/E submission gates remain open.

Private records and manifest SHA-256:

* ``hardware-build-20261017T182600.080209Z``,
  ``49b003c838f02c5f92287c53222abbd9ae7c2ba7b68143d5e583b30c3239498d``.
* ``load-ble-trial-20261017T182639.410161Z`` (empty VM),
  ``1012f1660280b1c2c4709cd8ef72d4cff1ca3c25603fbcc42fb9dd346bbd1e36``.
* ``load-ble-confirmation-20261017T221210.724824Z``,
  ``ecc3312b1cefc6709ec7c437750b6c319e2133c7642f40ea28344b5c177441a3``.
* ``load-ble-trial-20261017T221219.018687Z``,
  ``5f1dec77e9518414c33078c2be3583aa30ae11e1d8f625be657d1e45ebc726b4``.

Explicit native HCI physical-trial preparation (2026-10-18 SGT)
-------------------------------------------------------------

Source ``8a33ab101830`` adds opt-in ``native_hci=1`` to the tagged-VM BLE
experiment, after the same identity/startup/acknowledged-disable sequence.
Only that experimental owner publishes the real auxiliary child; btws73 and
the native Bluetooth stack perform the next enable and HCI initialization.
The default trial still ends at disable, and the regular physical parent
still publishes no child. Use the opt-in only with a freshly loaded epoch,
not an arbitrary retained runtime. Reference board inputs and address-source
qualification are not broadened by this test mode.

``hardware/build.py --hci`` builds into separate fixed kernel/module outputs
and keeps a separate evidence pointer. Both the native-HCI and default
Clang W=1 builds passed. Their empty VMs passed the existing admission/profile
checks and explicit registration/unload. The default build additionally
rejected ``native_hci=1`` before driver registration. The HCI VM opened a
native management socket and confirmed that no HCI device existed without
physical attachment. No physical USB was accessed in these checks.

The prepared continuous-load guest requests management Read Info only after
a configured index notification; no BlueZ daemon or radio test command is
started. Actual HCI command/completion pairs, feature/address/version agreement
with management information, four BLE class transactions and complete owner
drains are required by the new transcript verifier. A successful USB write or
HCI registration alone cannot pass. Its initial allowlist covers native
initialization/readout operations, not scanning, advertising or connecting;
unanticipated behavior will be retained as a failed trial for investigation.
Read BD_ADDR remains an observation, not proof of provisioned public identity.

Management's not-powered state can hide native setup's AUTO_OFF phase, so
the supervisor also waits for the second actual BLE_CLOSE notification before
detach. The transcript independently checks that close's class request and
reply follow HCI replies. Two socketpair-only management positives and 15
negative cases passed, including repeated settings without duplicate markers.
One synthetic HCI transcript passed; 24 independent mutations were rejected,
including missing captures/drop counters, errors/opcode mismatch, inconsistent
management identity, missing close-related markers and kernel diagnostics.
These are supervisor properties, not original-firmware HCI compatibility.

Preparation failures are retained in the private qualification summary:
GCC first rejected the enlarged module-parameter string's old buffer capacity;
the initial synthetic fixture shadowed an event helper; class-completion
matching wrongly assumed globally unique URB identifiers; one negative
mutation left its input unchanged; a private edit used the wrong working
directory; and sandboxed socketpair creation failed before parser execution.
The buffer, fixture, ordered URB pairing and mutation were corrected, the edit
was rerun, and the pure IPC tests passed with local sockets available. Neither
the initial empty-VM preflight nor the corrected preflight touched hardware.

The next physical HCI trial remains unexecuted and requires a fresh allocated-U
power-removal confirmation. Earlier real U BLE enable/disable evidence and its
scope remain unchanged. HCI/RF/peer, address provenance, E and formal joint
submission gates remain open.

Private records and manifest SHA-256:

* ``hardware-hci-build-20261017T222003.367490Z``,
  ``9020422b0fff4faa520f41b8f8880e94cf7cc2efed656bea677bf32937b070d9``.
* ``hardware-build-20261017T222526.640840Z``,
  ``72679b2477a3fa9374bf5ca711937f6f71ab60142e1158baad05e5443450c19a``.
* ``load-hci-trial-20261017T222625.005188Z``,
  ``f725dcd9ffd94c608c5508e3514a2ea0962cb4408d2fbce3025f64369fc4affd``.
* ``load-ble-trial-20261017T222606.021460Z``,
  ``7c3bef00e63f9e348cd05680d69633c1e578d5b8d8901dfb93b901820e3c604e``.
* ``native-hci-preparation-20261017T222842.377146Z``,
  ``6e6e680b6ed4d20a5d85b0ebfaa14a3d5c776e32970c3236376478ba162a410f``.

Explicit LE configuration and wire-based native close gate (2026-10-18 SGT)
-------------------------------------------------------------------------

Pre-execution review found two gaps in the preceding physical-trial
preparation. First, allnoconfig left CONFIG_BT_LE disabled even though the
native HCI driver and command initialization compiled. Source ``2202c0568ffc``
explicitly enables CONFIG_BT_LE and disables CONFIG_BT_BREDR in both the
hardware-HCI and virtual USB fixtures. Earlier records retain their actual
configuration and are not relabeled as complete LE-stack qualification.

Second, the USB owner logs each function-notification kind only once through
notifications_seen. A supervisor waiting for a second BLE_CLOSE printk could
therefore time out after a successful native close. The earlier readiness was
withdrawn before any physical execution. The corrected private supervisor
waits for actual capture records: four completed ordered class transactions
(25,26,25,26), with both complete BLE_CLOSE replies following their matching
requests. Text timestamps are joined to full binary receive payloads, and
reused URB identifiers are handled by ordered pending requests. Final complete
transcript validation still checks all transfers, HCI replies and owner drains.
Neither management's powered bit nor deduplicated printk output is a close ACK.

The new wire gate passed two synthetic positives and rejected 13 missing,
short, malformed, failed, wrong-device/queue/role/tag controls. It also rejected
the hash-verified earlier real U startup/disable capture as insufficient for
native HCI close. The HCI transcript checker covers read-buffer v1/v2 and the
native default-PHY/disabled-ISO-host initialization commands traced to
net/bluetooth/hci_sync.c. Two synthetic transcripts passed; 24 shared negative
types plus two extended-path mutations were rejected. This does not establish
actual firmware capabilities or permit radio test commands.

The corrected hardware-HCI build and empty VM preflight passed. The virtual
USB build and full/high-speed native HCI runs each passed three cases: two
initializations and Reset-in-flight cancellation, 27 commands/26 synthetic
replies, two management readouts/auto-off acknowledgements and three drains.
KASAN/UBSAN/lockdep diagnostics were absent. The virtual supervisor rejected
34 log and five artifact mutations. These are bounded synthetic integration
results, not physical HCI/peer qualification or resolution of prior KCSAN
reports. No new physical USB operation occurred; the requested fresh allocated-U
power-removal confirmation remains pending and must not be inferred from time.

Private records and manifest SHA-256:

* ``hardware-hci-build-20261017T223332.663004Z``,
  ``9275b75501df6ae08b0bcbae9a10368ebf1ccd0233a9f15caf6a37a9ecac91ec``.
* ``virtual-usb-build-20261017T223542.759832Z``,
  ``1baafdf16eecafd376a02d547c743b52bae2136c491e08b56b9a2df91a22d72a``.
* ``virtual-usb-20261017T223631.281003Z``,
  ``f637b64624501dd76099972c7a9519cdbd32b58db44af67dad26a888c565cbff``.
* ``virtual-usb-20261017T223703.034283Z``,
  ``963bdd88f9b042b63dffbcae4d12cc2b0601f402b6107b17c38b4bb06cd728fe``.
* ``virtual-usb-negative-20261017T223833.440080Z``,
  ``8a751726eca0eecba9d160ac00003481b39074f161de32b05cee20830f7b11ab``.
* ``load-hci-trial-20261017T223835.915555Z``,
  ``dc8c9e631ea2627147d4b676e85c5c62654e0154c76a6851935b637aad5d20de``.
* ``native-hci-le-correction-20261017T223929.335578Z``,
  ``854861f6ccc21ab94e88de9bc27c13d28e05ee1a668b0d72a2d8afe5b9e7a5cd``.

Allocated U native HCI observations and retained teardown failure (2026-10-18 SGT)
--------------------------------------------------------------------------------

After the user's fresh power-removal confirmation, the allocated U matched its
exact serial, port 1-1.1.1, unowned boot descriptors and host device 126. The
physical trial used source ``2202c0568ffc`` with documentation-only worktree
HEAD ``a0b947996aef`` and explicit CONFIG_BT_LE. HCC READM returned U/USB before
writes; the three original SDK110 files transferred 172580 payload bytes in
seven blocks after eleven preparation writes. The controlled QMP handoff
produced runtime host device 127 and guest device 3, with matching runtime
identity and versions. This remains a controlled transition, not natural
reenumeration qualification.

The actual auxiliary consumer registered native HCI. Twenty-three HCI commands
received matching successful Command Complete events, including Reset, local
features/version/commands, two consistent Read BD_ADDR replies, LE features,
buffer size, supported states, event masks, data-length settings and default
PHY. Management Index Added and Read Info returned one configured LE-only
controller. A nonzero address was observed; its provisioning source remains
unqualified. No scan, advertising or connection request was issued.

The independent binary/text checker found 32 complete runtime bulk replies,
matching transfer records and zero reported drops. Four class transactions
(25,26,25,26) and two actual BLE_CLOSE replies were present; the final close
followed the HCI exchange. All three device owners drained. KASAN, UBSAN and
lockdep diagnostics were absent in this bounded run. The earlier capture
limitation remains: only the first 512 bytes of the 864-byte customization TX
were captured, leaving its final 352 padding bytes unobserved.

The overall trial is nevertheless FAIL. After the supervisor deliberately
removed the runtime USB device, the guest observer treated management Index
Removed as an error and powered off. QEMU exited zero, but the host's subsequent
QMP quit raised BrokenPipeError. The unchanged archived HCI verifier also
rejects the explicit guest error marker after its preceding wire checks. The
original manifest, logs and consumed confirmation are retained unchanged;
partial observations do not replace the failed end-to-end result.

The private observer now accepts one correctly sized/indexed removal only
after management readout and the not-powered observation. The host separately
requires actual wire close, rejects removal or runtime drain before its detach,
and waits for Index Removed plus all three drains before quit. Command-status,
premature, malformed and repeated removal remain failures. Review also found
an inherited BLE-only ``boot + 8 OUT`` assumption in the boot checker; it now
checks the exact boot epoch independently of the strict runtime HCI verifier.

Three pure socketpair management positives and twenty rejection cases passed.
Two archived physical boot captures and four corrupted variants exercised the
boot filter. Two synthetic HCI transcripts passed, with 26 shared negative
types and two extended-path negatives rejected. The wire-close checker retained
two positives and thirteen negatives. A corrected empty-VM preflight passed;
no second physical attempt occurred. The VM exited, allocated U was unowned,
and the four SparkLink serials/device numbers remained unchanged. No hub,
controller or SparkLink operation was performed.

Another physical load requires a new allocated-U power-removal confirmation.
Native-HCI end-to-end qualification, RF/peer operation, address provenance, E,
Wi-Fi STA and formal joint submission gates remain open.

Private records and manifest SHA-256:

* ``load-hci-confirmation-20261018T022739.779832Z`` (consumed),
  ``b896278bbf509c7ba7af096dcc8af4197200e9bf4e7d2bcdc65369d8a414d830``.
* ``load-hci-trial-20261018T022751.089293Z`` (FAIL),
  ``e28df9d91cf44ceff0960ac02a5aa9c119bdf51e7c8f9da78b74234c56d8e580``.
* ``native-hci-failure-audit-20261018`` (partial observations, not a trial PASS),
  ``84acd675c77f7211deeee7bc5bf067fe6662cdba4c282484216fba0a5f5c0f20``.
* ``load-hci-trial-20261018T023024.554968Z`` (empty VM only),
  ``45e377d78af2f880eed9191305669ff59bbd09c97d4a3b4221e547e25adbfd18``.
* ``native-hci-postcheck-20261018``,
  ``14fcc8a2926a3225e86216b57e0f0a202fa5bab2a5aa0c2e477765c6570b6512``.

Native management removal regression (2026-10-18 SGT)
----------------------------------------------------

Source ``b5e5fe852012`` closes a related coverage gap in the hardware-free USB
fixture: previously it closed the management socket after USB drain without
examining Index Removed. Each of its two initialized-controller cases now
requires exactly one six-byte removal event for index zero after disconnect
and owner drain. Queued New Settings are consumed within a bounded wait;
other records fail. A further 100 ms event-free window precedes socket close.
The Reset-in-flight case did not reach a configured index and retains its
separate cancellation deadline instead of requiring this event.

Full-speed and high-speed KASAN/UBSAN/lockdep VM runs each passed three cases,
with two management removals and two event-free windows, 27 HCI commands,
26 synthetic replies, two initializations/auto-off acknowledgements, one
Reset cancellation and three owner drains. No sanitizer or locking diagnostic
was reported. The supervisor rejected 41 log and five build-artifact mutations,
including missing, duplicate, wrong-index, short, premature removal, omitted
event-free-window evidence and a spurious configured removal in the pending
Reset case. These exercise real native-stack events over synthetic USB, not
physical-device behavior.

A new hardware-HCI build and empty-VM preflight also passed from this clean
source. No physical USB device was attached in this increment. The earlier
physical HCI attempt remains FAIL with its 23 successful reply observations;
its confirmation stays consumed. A fresh user power-removal confirmation is
still required before another allocated-U load. No new RF, peer, address,
E, recovery or submission qualification is claimed.

Private records and manifest SHA-256:

* ``virtual-usb-build-20261018T023509.386669Z``,
  ``ffc5812a1da03c3b2af77e8b2ce6447ad0970ed83171ffc284fe2e6211a6a2cc``.
* ``virtual-usb-20261018T023527.811941Z`` (full speed),
  ``66877314a2438b059113b6de4b70bb73299501f4d559cf55bf428ad61a578281``.
* ``virtual-usb-20261018T023544.218130Z`` (high speed),
  ``6fbcb64b717491c83f3ae8fdcbc6ee5faa6408143b4d5d0d4b49a27f087946aa``.
* ``virtual-usb-negative-20261018T023545.318068Z``,
  ``c0423ae0e2d9dfc0ff325c9f93f77154214dfd1025ef940c68c96a6b469c6370``.
* ``hardware-hci-build-20261018T023552.654107Z``,
  ``68364f2ea05fd8a84e94eba3a5d277b0fe840a78511db2fb6652b85bcfb1ca79``.
* ``load-hci-trial-20261018T023621.226788Z`` (empty VM only),
  ``5f301b004ef127ecfe493cf39325de493519cd7bd590963aa308280b5b186f8a``.

Allocated U native HCI initialization and teardown pass (2026-10-18 SGT)
----------------------------------------------------------------------

The user's new power-removal reply was recorded as a fresh single-use
confirmation, with exact serial/topology, unowned two-endpoint boot device 4,
source/preflight hashes and a read-only snapshot of the four excluded
SparkLink devices. No prior confirmation was reused. The compiled source is
``b5e5fe852012``; worktree HEAD ``ffead2df06af`` differs only in documentation.

This physical run completed the bounded SDK110 path: boot READM returned
four-byte zero (U/USB) before writes, VERSION returned BOOTVER001, all eleven
preparation writes were acknowledged, and the three separate original files
transferred 172580 payload bytes in seven blocks before QUIT. The boot checker
validated 29 OUT transactions and 28 replies, including 26 full loader replies.
The explicit QMP handoff admitted a separately checked five-endpoint runtime
instance, host device 9 and guest device 3. U/USB and firmware/ROM versions
0x010a6e3c/0x00010101 matched before platform/customization and function startup.
Host device addresses need not be consecutive; the bounded host kernel log
records target boot disconnect, runtime enumeration and a target-device reset
on release, with no matched parent-hub or excluded-device event. This is not
natural reenumeration or recovery qualification.

The real auxiliary/HCI consumer completed 23 matching successful command/event
pairs. Commands were Read Local Features/BD_ADDR, Reset, local version and
supported commands, LE features/buffer size/supported states, event masks,
LE transmit-power and list-size reads, list clears, RPA timeout, data-length
reads/defaults, advertising-set count and default PHY. The transcript's exact
opcode list is bound to the private manifest. Management announced one
configured controller and returned matching address/version/manufacturer with
LE support and no BR/EDR. Both address reads and management agreed; agreement
does not establish how that address was provisioned. No vendor address write,
scan, advertising enable, connection or ACL business request was sent.

There were 33 successful runtime bulk OUT and 32 complete bulk IN records,
four completed class requests (25,26,25,26), seven function notifications,
and two actual close replies. Final close followed the HCI exchange. Text and
binary bulk captures matched with zero reported drops; all runtime replies
fit the 512-byte capture bound. The prior customization limitation remains:
its final 352 TX padding bytes were not captured.

After the actual final close, the supervisor deliberately removed the runtime
USB device. Management Index Removed was accepted once, all three owner
instances drained, and QEMU quit with exit zero. No KASAN/UBSAN/lockdep diagnostic
was reported. The overall status is
RAM_LOAD_CONTROLLED_HANDOFF_NATIVE_HCI_INIT_PASS_NOT_RF_OR_ADDRESS_PROVENANCE.
The prior first-native-HCI attempt remains FAIL and is not rewritten. This is
one successful initialized-and-closed reference-profile U instance, not the
20-start/100-cycle/24-hour qualification or completed BLE support.

A separate read-only postcheck confirms no project VM, no private QMP socket,
an unowned allocated U and unchanged serials/device numbers for all four
SparkLink devices. The confirmation is consumed. Ordinary parent automatic
loading/publication, board and address provenance, RF/BlueZ/peer communication,
E qualification, Wi-Fi STA, natural recovery and formal upstream submission
remain open. No new power cycle is requested merely to repeat this success;
a subsequent physical experiment must define its scope and establish a fresh
appropriate device state.

Private records and manifest SHA-256:

* ``load-hci-confirmation-20261018T032545.153903Z`` (consumed),
  ``5a7f76d24558a6d74c3c35bcab84ebc71e9cfccaa6e083487f0f481a6b9f6748``.
* ``load-hci-trial-20261018T032553.058168Z`` (scoped physical PASS),
  ``ddeb87e2cc056dd6f1ba24860296b36993ac013c10fe8ba6ba91257097b67960``.
* ``native-hci-success-postcheck-20261018``,
  ``39303e700ed427a422fe31d1fe083f24081a33ac190f76a2318fef38048f9762``.
* The same postcheck's ``host-window-manifest.json``,
  ``cc8170bddc7e2b82adbc2e3865eef2d6179ba727a0076ccde564d1943a7d67c1``.

Internal shared-core and BLE review extraction (2026-10-18 SGT)
--------------------------------------------------------------

An independent local ``ws73-review-core-ble`` worktree/branch now contains
snapshot commit ``074eed78f2cb`` on the original Linux base
``70dd4ee72eb2``. Its 70 selected files are byte-identical to development HEAD
``ca097044f211``: shared core, USB, actual BLE consumer, associated kernel
tests and the hardware-free USB reproduction fixture. Wi-Fi development files
and accumulated project-history documents are excluded. The development and
SparkLink histories were not rewritten. This single 12907-line snapshot is an
internal inspection artifact, not an individually buildable RFC patch series.

The snapshot compiled with Clang W=1 and four jobs in a separate fixed output,
with CONFIG_BT_LE/BT_WS73/WS73_CORE/WS73_USB enabled and CONFIG_WIRELESS disabled.
Its full-speed KASAN/UBSAN/lockdep VM run passed the three native-HCI cases:
two initializations, one Reset-in-flight cancellation, 27 commands/26 synthetic
replies, two management readouts/removals/event-free windows and three drains.
The supervisor rejected 41 log and five artifact mutations. No physical USB
was attached. Earlier physical U evidence remains bound to its development
source; this extraction does not inherit physical qualification or prove all
future split commits build independently.

The strict whole-snapshot checkpatch result is FAIL: 213 errors, 939 warnings
and 76 checks. Categorization assigns all 213 errors, 931 warnings and 32 checks
to userspace test tooling; kernel/include files have zero errors, six warnings
and 44 checks; two warnings concern patch metadata/MAINTAINERS. Findings are
retained for correction rather than hidden by excluding tooling from the
report. Ordinary-parent publication, board/address inputs and hardware gates
are also still unresolved, so a successful extraction build is not RFC readiness.

Private draft artifacts include an architecture cover letter, proposed split
boundaries, source diff and mail-format snapshot, source/artifact hashes and
baseline maintainer candidates. They explicitly identify the ordinary USB
binding as observation-only, the limited U result, absent E/peer/STA evidence,
and the need for a current target tree, human maintainer responsibility and
user authorization before any external send. Filename-based maintainer output
for drivers/misc alone omits USB-list review; the baseline USB SUBSYSTEM entry
is retained separately. No external mail, PR or RFC was sent, and the local
review branch was not published as a support series.

Private record ``core-ble-review-snapshot-20261018/manifest.json`` SHA-256:
``be1bfa101dadd97d5a3e6c4394a312374cbc6c6cbc5a34ac36fe71e2cf81887a``.
Its isolated validation records are under the review worktree's sibling
``.dev/ws73-upstream/records``:

* ``virtual-usb-build-20261018T033819.972784Z``,
  ``d042f767e062b223a4dee604edbba2dbe28b9157a596ff9d2a494bd12cc6cde5``.
* ``virtual-usb-20261018T034112.806937Z``,
  ``be9e5e5effb1ab7701d131a80762220dde70fe8770b1fa1f078eb1855bc07a28``.
* ``virtual-usb-negative-20261018T034130.650349Z``,
  ``af166296f61aca5bdd80063963c3216d1a187fefa5ddf21fbe7b4ee81c47f409``.

Kernel review formatting increment (2026-10-18 SGT)
--------------------------------------------------

Commit ``bf3c2801d9e1`` corrects the kernel-side findings from the frozen
internal review snapshot: six warnings and 44 checks. Four unchanged BLE
exports now follow their definitions; transaction/RX lock responsibilities
are documented; declarations and test expressions are aligned. Raw Clang
lexing compared 14417 tokens across 11 files after excluding whitespace,
comments and the separately verified export relocation. This does not claim
identical source locations or diagnostic line numbers.

Strict checkpatch reports zero errors/warnings/checks for this increment.
The current kernel-only extraction retains one MAINTAINERS metadata warning;
userspace-fixture findings and series splitting remain outstanding. The
frozen review snapshot and its original failed checkpatch result are unchanged.
Clang W=1 built the changed source and all 94 existing KUnit cases passed in
the x86-64 KASAN/UBSAN/lockdep VM. The first attempt compiled but could not
create its QMP socket inside the sandbox, ran zero tests and remains FAIL.
The second attempt used the same source/patch in the permitted VM environment.

Private record ``kernel-style-20261018/manifest.json`` SHA-256:
``4036dfbb300b3c60a3504302d0ed6de1002a5f933b74edc935ca324e6dd99557``.
KUnit PASS record ``20261018T035309.755441Z`` manifest SHA-256:
``ebaa4af357286a8cbf9d6e40b0fe03b5bcbad5b76e3c913ebe500e5e23991998``.

Four-device allocation and boot observations (2026-10-18 SGT)
------------------------------------------------------------

The user reassigned this thread U ports 1-1.1.3/1-1.1.4 and E ports
1-2.1.3/1-2.1.4. Hub 1-1.1 is 05e3:0610; hub 1-2.1 is 1a40:0101. Both share
0000:c5:00.3. Ports 1/2 on both hubs are exclusively the other thread's.
These are topology ports, not physical enclosure numbers. The now-excluded
1-1.1.1 and its earlier native-HCI PASS are not current test resources.

Read-only sysfs inspection found all four assigned devices at 12 Mbps with
unbound boot interfaces. Exact serials and descriptor bytes are retained
privately. Current-user fuser found no assigned-device owner; existing libvirt
XML showed only unrelated bus-7/bus-8 hostdev entries. Privileged fuser was
unavailable because sudo required a password, so this does not assert a
complete inventory of inaccessible process descriptors. No conflicting owner
was observed or interrupted. The other thread's enumeration changed during
inspection and was not claimed as stable or operated on.

Nine existing private runners now require explicit --device-port and --board,
share one exact four-port/serial allowlist and recheck topology, hub, descriptors,
node access and ownership immediately before each device_add. Device numbers
are freshly observed, never selectors. The current-U retained-runtime trial
is retired for hardware because its old epoch belongs to an excluded device.
RAM/BLE/native-HCI loading callers reject E before VM startup; the existing
ROM observation runner accepts both explicit profiles. SDK110 U and E three-file
hash/header checks passed separately. Download inputs/install paths derive
from the selected board, never from a global mixed image set or fallback.
Sixty offline admission checks passed, including all nine callers' rejection
of omitted and excluded targets. This is no replacement test framework.

The local build.lock was free, as was the existing cross-thread
.dev/main-build.lock. With both held, the former became an alias to the latter's
inode. A second open through the alias correctly failed admission. No lock
owner was terminated; the prior local file was retained. Existing builders
and VM runners now serialize through the same global lock, retaining four-job
builds and the combined 12/8-with-hardware-VM limits.

After an empty-VM preflight, four separate device-level VM observations used
source ``bf3c2801d9e1`` and the adapted existing runner. Each executed exactly
READM 0x40019000 4 and VERSION, with actual successful USB replies, one disconnect
drain and QEMU exit zero. Both U ports returned four zero bytes; both E ports
returned 0a000000, matching candidate U/USB and E/USB decoding respectively.
All returned BOOTVER001 plus two NUL bytes. No firmware or calibration file
was downloaded, no runtime GET_CHIP_TYPE was performed, and no hub/controller
operation was issued. All four remained unbound with no visible owner after
VM exit; no project VM/QMP endpoint remained.

Each result is OBSERVATION_PASS_NOT_COLD. The user power-cycle messages do not
establish per-device cold-state history for the newly assigned set. Cold-ROM
automatic selection, U/E startup profiles, runtime cross-checks, BLE, RF/peer,
STA, concurrent mixed devices and natural/non-artificial recovery remain open.
No new device inherits the former allocation's qualification or confirmation.

Private per-device records and manifest SHA-256:

* U 1-1.1.3, ``usb-observe-20261018T035618.234315Z``,
  ``63e4af03f3b34e3b0d471a278a204f5cb3df8a81da520751a59b15b4e1a94e2e``.
* U 1-1.1.4, ``usb-observe-20261018T035635.522623Z``,
  ``93d5b378b941d0b909e42ba99a2faa4942eed2207fd8aaa6343a4424419e3564``.
* E 1-2.1.3, ``usb-observe-20261018T035705.971985Z``,
  ``92dfa526c4322ccaa15dd7047312fcaaa1f3c8cc364060da651fd9b3d38d64bf``.
* E 1-2.1.4, ``usb-observe-20261018T035722.298050Z``,
  ``3cc57514412f8b1f7c6ac0c4e906201aa0c741e7185357acb1d28337f55650a1``.

Allocation, caller changes, firmware inputs, tests and inspection limitations
are bound by ``hardware-allocation-20261018/manifest.json`` SHA-256:
``71fbe064e0e578961ec839c05baf266bd409b08b3acf22288195fc7802ecbdc2``.

Explicit board identity before boot inputs (2026-10-18 SGT)
----------------------------------------------------------

Commit ``26e5cf57ddc8`` requires the expected U/USB or E/USB identity alongside
explicit board inputs in ws73_boot_start. It validates the expectation before
USB traffic, copies it for the attempt, and compares both package and bus
with the fresh HCC READM before firmware acquisition or preparation writes.
A mismatch is terminal at the identity phase, even if the caller subsequently
supplies a matching expectation. No automatic alternate-profile retry exists.
The physical reference trial remains explicitly U-only. The dummy_hcd-only
test driver takes an independent synthetic board selection from its fixture;
that test metadata is not a physical USB identity mechanism.

Two new KUnit cases cover seven live-identity mismatches (both U/E directions,
alternate U encoding, SDIO and UART) and five unsupported/missing expected
inputs. The first run caught an introduced ERR_PTR handling mistake: the
new admission check tested NULL instead of IS_ERR. That 95/96 result remains
FAIL. After correction all 96 cases passed with KASAN/UBSAN/lockdep. The tested
dirty patch exactly matches the committed source diff. Kernel change
checkpatch reports zero errors/warnings/checks.

The existing full-speed USB loader VM passed ten cases, including independent
U/E three-file loads, original-file integrity failures, wrong-bus rejection
before input acquisition, disconnect and terminal reuse. Its 22 FILES/payload
transactions and ten drains were checked; this is synthetic USB, not firmware
execution on a new device. The supervisor rejected 45 log and nine artifact
mutations. The physical-trial kernel/modules built successfully, and the
existing explicit-port caller passed an empty-VM preflight. No physical USB
was attached during this increment, and the E download gate remains closed.

Five SDK110 Host files were checked byte-for-byte against pinned source
``d5e68a58fe38``. They establish configured original-file paths/addresses,
conditional RAM2/3/RAM4 preparation and the reference INI trim/temperature
settings. The examined preparation routines have no U/E branch. This does
not establish a particular E board's trim, runtime version or BLE compatibility;
those remain required before extending the physical trial.

Private record ``explicit-boot-identity-20261018/manifest.json`` SHA-256:
``6634be983898ca4fffa93f04cfb8d84027fa015816671077809f777d312b7c61``.
Validation records and manifest SHA-256:

* Retained FAIL ``20261018T040536.021691Z``,
  ``ff761b202721def317a6202721daae6ef31d4a28c0a3f33f518551123edb5d09``.
* KUnit PASS ``20261018T040656.098498Z``,
  ``fd6033f029c19e655a09076d21608f987ed5b05723b690b445a51945be9024b6``.
* Virtual build ``virtual-usb-build-20261018T040738.629850Z``,
  ``780fa2f86b67cfe581878106892997cffe9da3a3e7c9102a72c62d1eadf5e399``.
* USB loader ``virtual-usb-20261018T040804.716863Z``,
  ``589051ec2a1bae75070113b9dd507cfb8b16a7ea40c3d5286b60509c30dd3c49``.
* Rejection controls ``virtual-usb-negative-20261018T040828.739467Z``,
  ``b5236c8b55bbcd8c8b051d17ef8b4af030a4497311713bbb42a6e6afb11ab4a0``.
* Trial build ``hardware-hci-build-20261018T040828.889117Z``,
  ``3ae26884b4d49e5fe6fa1791a6652930220ea3828bb76bb863e392d2246475ae``.
* Empty VM ``load-hci-trial-20261018T040915.538505Z``,
  ``4011584a05fe01d83b69a9d462cc1ecb7b4117382a2a58a2fca053fc310e4ae5``.

Selected-board RAM/runtime trial preparation (2026-10-18 SGT)
-------------------------------------------------------------

Source ``2f55834c88d2`` requires an explicit package=0 (U) or package=2 (E)
matching the specially tagged VM. The existing guest selects its expected
READM from a required board file; the loader independently matches fresh HCC
before acquiring firmware or writing RAM. Missing/mismatched tags do not
register a USB driver. PID 1, exact serial/enumeration, single guest port and
one-shot admission remain required. This extends the bounded observation
trial, not the normal physical driver's publication policy.

The existing private RAM runner selects only the requested board's original
SDK110 code/Wi-Fi/BTC inputs, checks hashes and payload sizes, and supplies a
separate per-device VM image. Its checker validates complete runtime chip and
version replies against the explicit board and driver observation; an unknown
E firmware version is recorded, not forced to the historical U value or
accepted for BLE. Hardware execution requires a fresh, unconsumed confirmation
for the same board, port and current device snapshot. No such E execution
occurred during this preparation increment.

Both profiles passed empty-VM checks: eight admission cases, eight package/tag
cases and failed module loads with missing device number/package or wrong
package/tag. No physical device was attached or trial driver registered.
Inspection of each actual initramfs found exactly its selected three original
firmware files, matching their full hashes, board file, serial and VM tag.
Each board's transcript verifier rejected 26 boot mutations and 143 runtime
reply mutations, including all reply truncations, cross-model/wrong-bus data,
malformed lengths, failure status, duplicate/reordered responses and driver/wire
version disagreement. Five CLI rejection checks and strict compilation of
three existing U guest callers also passed. Kernel checkpatch is clean.

Private preparation record ``e-runtime-path-20261018/manifest.json`` SHA-256:
``6014ad69c035460dcf05130db9c4f8d15fdddddc85d18ae560cf6275d630e64c``.
The archived current runner additionally allows the hardware README as a
non-executable documentation change since build; runtime behavior is unchanged
from the two preflights. Build and empty-VM manifest SHA-256:

* ``hardware-build-20261018T041501.221803Z``,
  ``5bb1bb7db9ef85f2beacbe4340218c8316687d63f7227c61eee61216bd84e341``.
* U ``ram-trial-20261018T041612.438166Z``,
  ``72838576d5c988d878c09a9de0b87bd857f6c5ed60168aa82f42f5c66b69f93e``.
* E ``ram-trial-20261018T041635.534754Z``,
  ``f00537455bea529cc0e5e153ca270a1fe779983586ddecbfd736442f972bed16``.

This does not qualify E loading, runtime, BLE, concurrent mixed devices,
reference trim as production provisioning, cold-ROM automatic selection or
natural/non-artificial recovery. BLE/native-HCI callers remain U-only. Earlier
U evidence stays tied to its original now-excluded serial/epoch; new devices
must earn separate evidence. All firmware files and private serials remain in
local records, not the public source commit.

First newly allocated E load: runtime transition failed (2026-10-18 SGT)
-----------------------------------------------------------------------

The user confirmed removal of all power from E at 1-2.1.3 and reinsertion into
that same port. A fresh exact-serial/topology/boot-descriptor/ownership check
bound confirmation ``e-ram-confirmation-20261018T042309.776323Z`` to this device,
not an earlier allocation. It was consumed once by
``ram-trial-20261018T042320.692535Z``, compiled source ``2f55834c88d2`` and
worktree ``0fc67bea04f6``. Only that individual device was attached to the
isolated VM; no hub/controller or other-thread device operation occurred.

Both pre-write READM replies were exactly 0a000000; VERSION was BOOTVER001.
The original E code/Wi-Fi/BTC inputs passed full hash checks, with payloads
137580/20996/34056 bytes. Eleven preparation writes and eight file blocks
(total 192632 bytes) completed before QUIT. The tagged loader reached
WAIT_RUNTIME. Independent post-processing checks all 31 OUT and 30 IN
completions, complete replies, command/chunk ordering and READY holdoffs.
Binary usbmon checks at most the first 512 bytes of each OUT against the
hash-verified input; it does not prove uncaptured payload bytes on the wire.
An initial offline audit incorrectly treated an interrupt record as bulk;
that audit failure is retained alongside the corrected bulk-only analysis.

After the controlled QMP boot detach, the 10-second runtime admission deadline
expired. The host recorded repeated full-speed enumeration, setup-address
error -71 and then the same serial with 12-Mbps/two-endpoint boot descriptors.
No runtime attachment, GET_CHIP_TYPE, version query, BLE or RF operation
followed. Both guest owners drained and QEMU exited zero. A post-exit check
found the same allocated device unbound and without a visible owner. The
original trial remains FAIL; confirmed download transactions are not a
successful firmware startup. Device startup versus controlled-handoff effects
remain unresolved, and no retry, firmware substitution or recovery was run.

Private failure manifest SHA-256:
``cdc5a18b1d99ad80f92bfb50e8ea51b93c0fe567c0435c9efa66a9f9fa058bc5``.
The separate analysis binds the unchanged failure, raw host enumeration,
post-inspection, consumed confirmation and audit:
``e-first-load-analysis-20261018/manifest.json``, SHA-256
``2e765469977e30ac1b323d6aefaa48f8a0bc30e6cc544f8dc98c34682c03ab0a``.

The existing U native-HCI trial was independently rebuilt for the package
parameter change and passed empty-VM preparation without any physical attach:

* ``hardware-hci-build-20261018T042248.714608Z``, SHA-256
  ``26c773ae4292598543a2ce1393dc4318ba33dc263f0e4d917ae3557bcbe1a7ba``.
* ``load-hci-trial-20261018T042454.209447Z``, SHA-256
  ``ca04ddf0c6f1691484536ebefe0926c8f226380713d29e99a0c1d29a4e9db208``.

The next E step is to examine transition timing and ownership against the
existing reference path before preparing another bounded trial. This result
does not qualify E runtime, mixed devices, cold-ROM automatic selection or
natural/non-artificial recovery. All project test VMs have exited.

Reset-disabled reference handoff preparation (2026-10-18 SGT)
------------------------------------------------------------

Read-only comparison found a material difference in the VM transport. Upstream
QEMU v10.2.2 hw/usb/host-libusb.c calls libusb_reset_device in close, open-failure
and exit paths. The installed usb-host property list has guest-reset controls
but no host-reset opt-out. The first E attempt used that system QEMU; its trace
cannot establish whether a particular reset caused the observed boot return.
Source: https://raw.githubusercontent.com/qemu/qemu/v10.2.2/hw/usb/host-libusb.c

The historical E software-reset observation used a different QEMU executable
with host-reset, guest-reset, guest-resets-all and kernel-driver-detach all
false. Its current binary, source and object hashes match the archived build;
the binary also matches that historical trial's prepared-input hash. A private
copy, including checked PC boot assets and a manifest of resolved libraries,
now lives inside this project's own directory. The shared reference executable,
build tree, VMs and hardware were not changed. The copied executable SHA-256 is
``05a3b654eb2801ed1ddb60fc6c6fa9bd5cd48eefa8194f999170eab90b26185a``.

The existing RAM runner now requires that pinned profile with no PATH fallback.
Each attach supplies the four false options and then checks their actual QOM
values. The common private attachment helper rejects callers that have not
verified this profile; older BLE/HCI/observation callers are consequently not
eligible for another hardware attachment until adapted and preflighted.
This withdraws their execution path, not their historical evidence.
The runner records monotonic/wall timestamps for QMP messages, USB lifecycle
trace events, and at most 256 changes in selected-port sysfs observations
around the controlled handoff. Observations never replace strict admission.
The runtime deadline stays absolute and does not renew on an observation error.

Twenty-two offline helper checks passed, covering unverified executables,
changed/missing executable or libraries, strict ports/board matching, false
reset options, and missing/inconsistent observation data. Both selected-board
empty VMs passed admission/profile guards and QMP property discovery without
any physical device_add; USB lifecycle logs were empty as expected. The
unchanged wire verifier again rejected 26 boot and 143 runtime mutations per
board. A hardware-only post-add QOM check is prepared, not exercised by those
empty VMs. This is not a new E firmware-startup result.

Preparation record ``reset-free-transition-preparation-20261018/manifest.json``
SHA-256 ``35508f5bcf3018af1c2f283ec973b84475504925f80473054f6656fb704d372f``.
Empty E ``ram-trial-20261018T043127.103547Z`` manifest SHA-256
``6ecacce428fd1f8c93a3ef38b61c5b2333a668e89ac8b3f65da1cf5ddedebc03``;
empty U ``ram-trial-20261018T043222.869192Z`` manifest SHA-256
``08d047b71b2e294045b4bcedf7501346c232f5811cf9ee218a4b6aad9052ca0b``.

The reference also differs in QEMU version and autoscan behavior. A subsequent
success would not isolate reset as the sole cause of the preceding failure.
Controlled QMP removal/reattachment remains artificial; natural transition,
recovery and new E runtime/BLE qualification remain open. Another load requires
fresh device-state confirmation; the first E confirmation remains consumed.

First allocated E runtime identity/version observation (2026-10-18 SGT)
-----------------------------------------------------------------------

The user's short reply confirmed the pending request to remove/reinsert power
on E 1-2.1.3 for the reset-disabled comparison. The confirmation records that
reply verbatim and its specific question context, rather than inventing a new
verbatim supply statement. The same device had already been explicitly
confirmed as having no other supply. Fresh admission observed the exact
allocated serial and boot layout at a new address; confirmation
``e-reset-free-confirmation-20261018T043651.869329Z`` was consumed once.

``ram-trial-20261018T043702.578518Z`` passed the bounded original-three-file load
and controlled runtime observation, with compiled source ``2f55834c88d2`` and
worktree ``1f12994627a4``. The fresh boot reads matched E/USB; 11 preparation
writes and eight blocks carried 192632 payload bytes. After QUIT and explicit
QMP boot removal, the same serial disappeared and returned as a 480-Mbps,
five-endpoint device. Admission waited for its node access before attachment.
A new runtime owner obtained complete GET_CHIP_TYPE and version replies:
package 2, USB bus 0, firmware 0x010a6e3c and ROM 0x00010101. These values agree
between raw wire parsing and the driver, without applying a U-version guess.

All eight actual per-object QOM reads reported the four reset/detach controls
false across boot and runtime objects. The lifecycle trace contains two opens
and two closes for only the selected device's two addresses and no reset
trace event. Three owners drained and QEMU exited zero; post-exit inspection
found the same runtime descriptor and no visible owner or host driver binding.
No BLE startup, HCI publication, radio request or peer traffic occurred.

The original trial manifest SHA-256 is
``f542652302e75a195ef28be534cc20883c2d8b063fbcb852e7928a2cb5c0335b``.
Independent replay of archived inputs and parsers checked the full transcript,
material hashes, QMP choices, returned identity and lifecycle trace. An initial
offline replay import-path error is retained; the corrected replay explicitly
loads the archived allocation helper. Analysis, host enumeration and current
inspection are bound by ``e-runtime-pass-analysis-20261018/manifest.json``:
``3e094f259f732548514a324c78618b980a28ee7009b7658921426819b69a406d``.

The first system-QEMU E attempt remains FAIL. Version, autoscan and reset policy
all differ between those transports, so this success does not prove a sole
reset root cause. The result is one controlled startup observation on one new
E sample, not natural recovery, cold-ROM automatic selection, general E board
provisioning, BLE qualification, mixed-device operation or formal support.

The existing U native-HCI runner now uses the same pinned private QEMU profile,
per-object opt-out checks and bounded transition observation. Its empty VM
``load-hci-trial-20261018T043555.655455Z`` passed with no device_add; manifest
SHA-256 ``8b09d21ef0bc8c147e65d2cb5d9c8cc129ae324b59a25dd02147f00659c1dd91``.
Five CLI rejection checks also passed before creating any VM/record, including
E requests and missing fresh hardware confirmation. Preparation manifest
``hci-reset-profile-preparation-20261018/manifest.json`` SHA-256:
``6a795546f18b9cb6001615853181a422a3f3a379262003f10ffc71b57fa5e7e2``.
Only RAM and this U native-HCI caller have adopted the new execution profile;
other older callers still reject physical attachment. E BLE requires explicit
package/tag/runtime admission and a separately verified continuous guest path.

Selected-board BLE trial admission preparation (2026-10-18 SGT)
---------------------------------------------------------------

Source ``3fb568f99daf`` extends the explicit tagged-VM BLE trial with a required
package parameter. U/package0 and E/package2 must agree with their DMI tag and
fresh runtime HCC identity before customization or enable. Both separately
observed SDK110 versions are pinned to firmware 0x010a6e3c, ROM 0x00010101.
Unknown packages, non-USB identity and any version-bit difference are rejected.
The 140-case pure identity test covers both positive identities, cross-package
and bus errors, unsupported/missing package, null inputs and each version bit.
E native-HCI publication remains forbidden; existing U HCI callers pass package0.

The existing continuous BLE runner and guest now select exactly one us/e
three-file image set and require matching trial-board metadata. There is no
combined image, cross-profile fallback or enumeration-order selection. The
runner adopts the pinned private reset-disabled QEMU, four per-object QOM
checks, timestamped QMP and bounded transition observations already used by
the successful E RAM observation. It requires a new port/board/device-bound
power-removal confirmation before each physical load. The private reference
140-byte INI customization remains a bounded trial candidate, not production
board provisioning. The SDK110 common USB BLE customization/enable call chain
was checked against its pinned commit; no E-specific override was inferred.

Both non-HCI and HCI four-job builds passed under the shared global lock.
Empty E ``load-ble-trial-20261018T044857.800877Z`` and empty U
``load-ble-trial-20261018T044914.661132Z`` each passed the 140 identity cases,
existing profile corruption/length cases and actual invalid module admissions,
then registered/unregistered the empty trial driver. Neither attached a USB
device. E manifest SHA-256:
``4828331b12ad5bf5718bd08c9f6b5d92e34de51ee44d332584b322508d67a0f2``;
U manifest SHA-256:
``3076b63d13470ddf4c2d67f24719997a57a8529413228c68e75e7a0dfc7b0f6a``.
The updated U native-HCI caller also passed its empty VM, recorded separately
as ``load-hci-trial-20261018T045024.326128Z``. This is compatibility preflight,
not another U hardware qualification.

For each selected board, the BLE verifier accepted one synthetic transcript
and rejected 12 damaged variants plus three wrong-board requests. The boot
verifier accepted archived boot traffic with four synthetic runtime OUT
placeholders, and rejected five damaged variants plus three wrong boards.
These fixture checks do not qualify hardware or the placeholder runtime bytes.
Five invalid CLI requests were rejected before creating a run directory.
The initial E boot-fixture mutation failed because grouped text hex was not
normalized; that failed check and corrected U/E reruns are retained. Initial
syntax checks encountered sandbox ccache access; repeated checks disabled
ccache. The first E preflight stopped at a guest format-truncation compiler
error before starting QEMU. Expanding its parameter buffer from 160 to 192
bytes resolved that failure; the successful preflights use the corrected file.

Preparation, before/after private callers, SDK audit hashes, all build and VM
manifests and limitations are bound by
``e-ble-preparation-20261018/manifest.json`` SHA-256
``943ea0cf040440ee4c0c633e558e1158a3d609d53e1f20f4fc133a4378421c9d``.
At this point a fresh E power-removal confirmation has been requested for one
bounded BLE startup/enable/disable experiment. There is no new E BLE hardware
result yet. All earlier failures, board/address/RF and peer gates, mixed-model
qualification, natural recovery and U/E joint formal submission remain open.

E publication rejection with HCI compiled in (2026-10-18 SGT)
-------------------------------------------------------------

The preceding E empty VM used a non-HCI kernel, so its native_hci=1 rejection
alone did not isolate the package restriction from the disabled build option.
The existing continuous runner now has an E-only rejection-preflight option
which selects the already qualified HCI build and forbids physical attachment,
confirmation input and U selection. Three CLI checks verified those rejections
before a run directory could be created. Normal hardware mode is unchanged.

``load-ble-trial-20261018T045442.112691Z`` ran source ``3fb568f99daf`` with
CONFIG_BT_WS73=y and E DMI. The real module initializer rejected native_hci=1
with ENODEV; the ordinary E trial still registered and unregistered correctly.
The 140 identity cases passed. No device_add occurred, the physical USB trace
was empty, no HCI device was published and QEMU exited zero. This verifies the
package gate independently of the build gate, not E HCI functionality.

VM manifest SHA-256:
``71eac56d0128f4a8125a765309e2078e9055de51ac226183b51e59be25802b60``.
Before/after caller and rejection evidence are bound by
``e-hci-rejection-preflight-20261018/manifest.json`` SHA-256
``7ade58b8b426810d50311ea4d1cbb4509ba77afd47929443847bad06fd9d1297``.
The pending physical E BLE experiment still requires its new power confirmation.

Split review builds and mock-client isolation (2026-10-18 SGT)
-------------------------------------------------------------

The original single-snapshot worktree remains frozen. A separate local
``ws73-review-series`` worktree now contains four dependency stages and an
explicit corrective fifth commit: protocol/firmware foundation, USB owner and
auxiliary endpoint, native HCI consumer, reproduction tools, then mock-client
test isolation. Each commit built independently with Clang W=1 and four jobs
under the shared lock. The final review commit is ``779800547f83``; its 70
selected file blobs and modes match development ``12db586c4c87`` exactly.
The branch was not pushed or sent as an RFC. Wi-Fi and private hardware inputs
are excluded. Patch sizing, message wrapping, userspace-fixture style,
MAINTAINERS, current maintainer-tree selection and actual support gates remain
open. The first four checkpatch results remain failures; the corrective fifth
patch has zero errors, warnings or checks.

Runtime validation exposed an actual test-configuration conflict. The initial
exec-only run had KUNIT_AUTORUN_ENABLED disabled and ran zero cases; that is
FAIL. With the documented kunit.autorun=1 parameter, 96 cases ran: 87 passed
and nine failed. Native btws73 bound the auxiliary endpoints before their
mock clients, causing EBUSY and unexpected native HCI traffic in the mock
backends. These failures are preserved, not relabeled as transport defects or
physical failures. The ordinary physical driver was never exercised.

Development ``12db586c4c87`` separates endpoint/link mock suites into
WS73_BLE_MOCK_KUNIT_TEST, requiring BT_WS73=n, including no native module.
Protocol tests remain available with the actual driver. The existing runner
has a combined-HCI mode that runs protocol and native HCI suites together;
default/core-only modes retain all mock cases. Actual Kconfig generation
verified native n/y/m: mocks resolve y/n/n while protocol tests remain y.

The default sanitizer VM passed 96/96 cases, including all ten mock cases;
the combined protocol/native-HCI sanitizer VM passed 100/100. Missing cases,
SKIP and unexpected suites still fail supervision. The repaired review kernel
passed its configured 86 protocol/codec cases with the real consumer compiled
in and mock suites disabled; it does not contain the fourteen native HCI test
cases that the development combined mode additionally runs. No test is removed
from the aggregate coverage, and none of these VMs attached physical hardware.

Split-series manifests, source correspondence, tool hashes, failed attempts
and final review runtime are bound by
``core-ble-review-series-20261018/manifest.json`` SHA-256
``5e4f3a8c608026e0198ec11b510f9c9f84511489f1cf0c142c3f433caebe269f``.
The development repair, Kconfig matrix and regressions are bound by
``ble-mock-isolation-20261018/manifest.json`` SHA-256
``11e27175b6af8e362ddce8fa9c5dd07d5f7e5e6401156a07028fd3fb8418978b``.
Default VM ``20261018T051127.229030Z`` manifest SHA-256:
``93d587dba339f310ff630f510065c4663d2f03bc132fa9ceb5bd682807da6c18``;
combined VM ``20261018T050741.588103Z``:
``28c8895c4c61dec7f1936df4b33e28a1e224c7421e0031553b1140bfafb30849``.

The E BLE image was rebuilt from the repaired source and its empty preflight
``load-ble-trial-20261018T051546.757264Z`` passed; manifest SHA-256
``022302a6d2d1a741afff4925d1babeb1db1e44003ec21aa8afc3fed10cf859cb``.
No new power confirmation has been consumed and no new E BLE hardware result
exists. The previous HCI-build evidence retains its original source and must
be refreshed before any future HCI hardware trial. Natural recovery, peer/RF,
board/address, Wi-Fi STA, mixed-model and U/E formal-support gates remain open.


Synthetic USB fixture formatting (2026-10-18 SGT)
------------------------------------------------

Development ``91b8a485eace`` expands the existing userspace FunctionFS fixture
for review. Its 10,329 non-comment raw C tokens match the preceding source
exactly. Runtime, boot, download, loader, startup and HCI fixture variants all
compile with GCC -O2 -Wall -Wextra -Werror. These executables were not run on
the host. This is formatting evidence, not a new USB or hardware qualification;
source locations and diagnostic line numbers can change.

The strict file check now reports zero errors, twelve warnings and eight
checks. Remaining findings include the userspace signal flag, descriptive
log text, existing array qualifiers, guarded strcpy, branch braces, nesting
and long log literals. Structural refactoring is separate; full checkpatch
cleanliness and RFC readiness are not claimed. The earlier frozen review
series and its original failing checkpatch reports remain unchanged.

``usb-fixture-style-20261018/manifest.json`` SHA-256:
``ca581af235873ef085f4bb206552125bf913aa3e6c5d37b82b417c07c00e5332``.
The non-HCI hardware build from this commit passed; manifest SHA-256:
``36f5acf7dc2bbf5bb8bdc38d051bcbcf926b3f651001e2a44b7f6e756dd3c559``.
The refreshed E empty-VM preflight ``20261018T052600.053696Z`` passed;
manifest SHA-256:
``616cfcef4b55fac4a2cbfa9d63a414f4b608a7cb1514045082e780a7244be691``.
No physical device was attached in this preflight and the VM exited.

A new user power-cycle report was recorded, but read-only checks still found
the exact selected E at its previous runtime enumeration, with no new kernel
USB disconnect/re-enumeration after 12:37 SGT. The report has not been consumed
for a hardware trial. Device/time clarification is pending; no new E BLE
hardware result is claimed and no hub, controller or other-thread device was
operated. Existing cold-selection, board/address, peer/RF, mixed-model and
natural-recovery gates remain open.


BLE control operation sequences (2026-10-18 SGT)
----------------------------------------------

Development ``be944aee4f81`` extends the existing BLE control KUnit fixture
with four fixed-seed campaigns of 4096 operations each. The production
set/receive/abort functions are called through the existing injected transport;
no production intermediate state is assigned. The oracle tracks unknown/on/off
status and the first terminal error. It checks every return, stable state,
first-error preservation and exact transport call count across acknowledged
actions, duplicate/wrong/late replies, short TX, cancellation, unsolicited
notifications, aborts and fresh isolated instances.

Across the four seeds there are 422 successful actions, 9732 rejected actions,
2028 notifications, 2076 aborts and 2126 fresh-instance operations. Each seed
must exercise all five categories, with their counters summing to 4096.
The existing supervisor requires the case and counters in default/core/combined
runs and rejects them in HCI-only runs. Eleven negative log controls were
rejected, including missing/duplicate/unknown seeds, truncation, malformed or
negative counters, zero coverage, wrong totals and unexpected campaign data.

The initial dirty implementation passed all 97 default KASAN/UBSAN/lockdep
cases, including the mock-client suites. Its precise patch is archived; this
was before the final test formatting/local event-variable cleanup. The clean
signed commit passed all 101 combined protocol/native-HCI cases; mock-client
suites are disabled in that configuration as previously documented. Counts
must not be added as independent cases. No sanitizer/lockdep diagnostic was
reported. Strict checkpatch on the final modified C file reports no findings.

Default run ``20261018T052958.571017Z`` manifest SHA-256:
``563318dcdcb14f3eac4d68b8d7c1f67d8ee59f869c17725c9d267ca48a9f36e4``.
Combined run ``20261018T053112.656160Z`` manifest SHA-256:
``3758309d7eeea2013897644c4e4da10d24513e4710b71e45d177ee856c689ea2``.
The aggregate ``ble-control-sequences-20261018/manifest.json`` SHA-256 is
``eb187185b9bdf56f44a55c192a91f57a032592d3ed65d1679b4b06f37616d25d``.

These are bounded single-threaded state-sequence tests, not coverage-guided
fuzzing, proof of arbitrary schedules, tokenless wire-epoch identity, real
BLE enable/disable or safe live-device reset. No new production defect was
established, so production control logic is unchanged. Earlier KCSAN reports
remain unresolved and no hardware qualification is inherited from these VMs.

The E non-HCI build was refreshed from the same signed commit (manifest
``711b8be022fb45c8e9b8637c3c8c494c9769be81f4409ae098b70b851b19f0fe``).
Its empty preflight ``20261018T053454.784667Z`` passed (manifest
``2145179bf1c0a51d6d760c8f31975600f27a98dd3ad09176b0a63b142df4d08a``).
The VM exited without physical attachment. The selected E remained in the
previous runtime enumeration during this work; device/time clarification is
still pending and no new power report was consumed for a hardware trial.


Allocated E native HCI from an initial boot observation (2026-10-24 SGT)
========================================================================

This is a new physical result for allocated E port 1-2.1.4, independent of
both the earlier E startup-only sample and the historical U HCI result.
Kernel/module source is clean e5a4274d2f203d8e128d1c5da4b6b7a790c344a3.
The run did not require manual replug and is explicitly NON-COLD: power
history is unknown, and pristine memory is not established.

The private supervisor's new initial-boot entry is restricted to this one
sample. Its pinned earlier observation contained only complete READM and
VERSION exchanges. Current serial, topology, boot descriptors, enumeration
consistency and lack of ownership matched that observation, and no other
recorded mutating or unknown sample use was allowed. The guard rechecked
these conditions before attachment. It preserves the original power-confirmed
route for other devices and later attempts; this is not an attestation of
all past activity outside this project's records or proof of cold-ROM validity.
Twenty-three function checks and five CLI rejection cases passed, including
changed identity/epoch, corrupted evidence, prior physical use, malformed
history and false no-hardware records. An empty VM then passed without any
physical device_add before the single physical experiment.

The actual flow reused the complete qualified experimental path: fresh HCC
READM returned four bytes representing 0x0000000a, the ROM version matched,
and only SDK110 E original ws73.bin, wifi_cali.bin and btc_cali.bin were
installed in the VM. All input hashes/roles passed before 11 preparation
writes, eight FILES blocks carrying 192632 payload bytes and QUIT. The
controlled boot/runtime QMP handoff created a new runtime owner, which
queried E/USB, firmware 0x010a6e3c and ROM 0x00010101 before customization.
BLE enable/BOOT and acknowledged disable preceded native HCI publication.

Native Bluetooth completed 24 actual HCI commands and matching successful
replies, including setup and kernel initialization. Management Read Info
agreed with the returned controller address; native auto-off received its
second wire BLE_CLOSE acknowledgment before the commanded removal. The
raw HCI version is 13 and manufacturer field is 0x0c7a; these observations
do not qualify additional PHY, ISO/audio, pairing or RF features. Address
origin/provisioning remains unqualified. The VM exited zero, all three
callback drains completed and the same allocated device was released in
runtime. There was no second HCI controller or physical mixed-device test.

Only hostport 2.1.4 was attached, once for each boot/runtime epoch. Eight
QOM checks confirmed the four reset/host-detach opt-outs were false. QEMU
trace contains two successful opens, two closes and no usb_host_reset.
The guest xHCI was emulated; no physical hub or controller was passed through,
reset or power-cycled, and the other thread's ports were not operated.
A replay with the original boot snapshot is now rejected because this
sample has a recorded physical loading attempt; there is no automatic retry.

The boot transcript checks command/payload prefixes and exact successful
OUT lengths/ACKs, not every firmware byte on the wire. Full original inputs
are independently hash-pinned. Runtime RX/HCI records are checked completely;
the existing final 352 customization TX padding bytes remain uncaptured.
All raw records remain private, including firmware-bearing captures.

Actual run: load-hci-trial-20261024T044509.241995Z/manifest.json, SHA-256::

  164d8a6cd791cda90f26e2e0d02fdd4f7393bc1423133cdaef399521991279ce

Current-source HCI image: hardware-hci-build-20261024T044121.069366Z.
Empty VM: load-hci-trial-20261024T044306.606984Z, manifest SHA-256::

  e57ac2e5763db6e8b25ea74fd80920e0dc9ce365e557fa5d78ad3fcf7baea105

This proves one E sample's initial-ROM-to-native-HCI experiment under the
recorded reference profile. It does not establish cold automatic selection,
production board/address policy, scan/advertise/connect/GATT or peer traffic,
natural transition/recovery, repetition or formal U/E support. No firmware,
board profile, peer requirement or upstream acceptance gate is closed by it.


Allocated U native HCI with independent initial-ROM admission (2026-10-24 SGT)
==============================================================================

Allocated U 1-1.1.3 now has its own physical result. It does not inherit the
historical U result at the now-forbidden port or the E 1-2.1.4 result.
The actual image source is e5a4274d2f203d8e128d1c5da4b6b7a790c344a3;
the worktree was d2c1b39a6025, with only the two separately recorded RST
changes after that build. No kernel/module executable changed.

The host admission has separate pinned U and E observations and expected
READM replies. U's fixed capture proves a successful four-byte zero reply,
not an empty/error read or a zero-initialized buffer. Four older manifests
mention this U serial only in other-device read-only inventories; their exact
reviewed hashes are allowed as census evidence, not as power confirmations.
Unknown records or changed census contents still reject admission. The old
confirmation names another physical U and is never reused for this sample.
Forty-six function checks and six CLI rejection cases passed, including U/E
separation, evidence changes, prior use, false no-hardware records and the
already consumed E route. The U empty VM then passed with no device_add.

This was explicitly a NON-COLD experiment: power history and pristine memory
are unknown. Immediately before individual attachment the current boot
identity/topology/ownership and recorded history were checked again. Fresh
in-VM HCC READM returned the complete zero word and the expected ROM version.
Only the US original ws73.bin, wifi_cali.bin and btc_cali.bin passed complete
input validation before 11 preparation writes, seven FILES blocks and QUIT.
Their payload total was 172580 bytes after removing three digest headers.
Twenty-nine boot OUT operations and 28 boot replies passed the existing
prefix, actual-length, timing and ACK checks.

The new runtime object independently returned U/USB, firmware 0x010a6e3c and
ROM 0x00010101. Reference configuration and acknowledged BLE enable/BOOT/
disable preceded HCI publication. Native Bluetooth completed 23 real HCI
commands and 23 successful replies, management Read Info, actual native
close acknowledgment and management removal. Three callback drains and
VM exit zero passed. The raw HCI version is 13 and manufacturer is 0x0c7a;
no additional radio capability follows from those fields. The U and E
observed addresses differ, but their origin, provisioning and global
uniqueness remain unestablished. No raw address is published.

Only hostport 1.1.3 was attached, in two boot/runtime epochs. All eight QOM
reset/detach checks returned false; the QEMU trace has two successful opens,
two closes and no usb_host_reset. The same device was unowned in runtime
after exit. Its initial route is now consumed; even a replayed old boot
snapshot is rejected by the recorded physical attempt. There is no fallback
to another port, firmware set or earlier U qualification.

Actual run: load-hci-trial-20261024T051421.864660Z/manifest.json, SHA-256::

  65fbca8bb777ccf5b1e94b039b495f9c6504f498c09eefc51ae3355cd0438ad1

U empty VM: load-hci-trial-20261024T051144.379393Z, manifest SHA-256::

  ab32efad4afa2d93f96ccddf53efbeb60674d42364f68dcf161fcb59d058c7a2

The unchanged capture limits apply: boot command/payload prefixes and full
completion lengths/ACKs are checked, not every firmware bus byte. Complete
runtime RX/HCI is checked; final 352-byte customization TX padding is not
captured. Original files and firmware-bearing raw logs remain private.

Together the two runs establish independent, sequential U/E sample HCI
initialization on the stated image and reference profiles. They are not
simultaneous isolation, real ACL/L2CAP/GATT traffic, BlueZ scanning or peer
interop, cold automatic selection, board/address qualification or natural
recovery. They do not qualify the separate char-misc-next review tree or
close the formal joint U/E submission, repetition or long-duration gates.


Allocated U/E runtime reattachment observation (2026-10-24 SGT)
===============================================================

After the separately recorded native HCI closes, allocated U 1-1.1.3 and E
1-2.1.4 were each attached to a new observation-only VM without replugging.
The ordinary USB parent used image source e5a4274d2f20; the worktree was
clean d50cef0b7856, with only documented RST changes after the image build.
No loader/function module or firmware input was included in either initramfs.
These are new runtime query observations, not another HCI initialization.

Each device returned its expected U/USB or E/USB identity and firmware
0x010a6e3c / ROM 0x00010101 from fresh HCC replies. Each capture has four
successful bulk OUT operations, two complete identity/version replies,
one cancelled pending bulk read and one cancelled interrupt read. The four
OUT operations are two finite queue controls and the two queries. Complete
binary captures additionally validate all 328 OUT bytes, including command
fields and padding beyond the text prefix, against independent expectations.
The two replies are cross-checked with text capture and decoded identity.

The interrupt read remained armed for at least 6.5 seconds after the version
observation, longer than the existing six-second BSP startup wait. Neither
sample returned a successful interrupt or another BSP_READY in that window.
No notification is invented from an old owner or a successful identity query.
This bounded absence does not prove that readiness can never recur; it also
does not establish the startup contract needed for functional warm recovery.
The existing BSP gate remains in place and no BLE enable was attempted.

Each VM exited zero, the callback drain completed, and the same physical
device was unowned in runtime afterward. Each attachment used only its
explicit hostport, with all four reset/detach opt-outs false, one QEMU open,
one close and no usb_host_reset. No hub/controller passthrough or operation
on the other thread's devices occurred. The image has no KUnit or synthetic
USB drivers enabled; the native HCI driver was built in but no child was
published by this observation-only parent.

The first U assessment rejected virtual-root-hub class management traffic;
the first E assessment then exposed the old verifier's data OUT endpoint 2
assumption, whereas the pinned runtime descriptor uses endpoint 0x01.
Both original manifests remain FAIL. Their complete captures were separately
replayed after correcting target filtering and endpoint checking. The two
later bounded trials below passed the supervisor directly. Forty-one
negative verifier controls passed, including missing/altered captures,
class/vendor actions on the target, wrong endpoints, false BSP reports,
interrupt faults and changes to otherwise hidden command/padding bytes.

U actual run: runtime-observe-20261024T055951.956102Z/manifest.json, SHA-256::

  64dd0e5ef114a9e2ba7f056c1020ebf4334ccdc515d6bca4123cfabb7d8ab061

E actual run: runtime-observe-20261024T060039.603625Z/manifest.json, SHA-256::

  65946168e5aa6a1194d3ac20ea91c7d532a174cf2541f3e704298437076d01de

Whole-TX checking was added by explicitly attributed offline postprocessing;
it does not rewrite either original manifest or claim another physical run.
Raw captures remain private. This work qualifies runtime identity transport
and clean detach for these separate samples only. It does not close cold
selection, firmware/address/board policy, functional warm recovery, natural
recovery, simultaneous isolation, peer/RF, repetition or formal U/E gates.


Fixed runtime reset-register read on allocated U/E (2026-10-24 SGT)
===================================================================

Source 4e0a63b50155da4576ade6c4d5b3575df3d1de2c adds a separate
inspect_reset=1 mode to the existing tagged-VM BLE trial module. It reads
only 0x4001f118 through the runtime register endpoints. It has no arbitrary
address, register-data-write or reset operation and rejects native_hci=1.
This prepares the register transport investigation; it is not recovery.

The mode retains explicit DMI/package/serial/enumeration/PID 1 admission.
It verifies live chip and SDK110 firmware/ROM identity before accessing the
fixed register, requests no firmware or customization profile and publishes
no function child. Each read is one fixed 16-byte setup on endpoint 0x02,
then one exact four-byte reply on 0x82. An absolute deadline spans both calls,
each I/O is capped at two seconds, and errors/cancellation/short or oversized
results leave the output unchanged. There is no retry or fallback value.
The existing owner/URB cleanup handles the two independently bound objects.

The existing pure guard-test module executes 24 cases without USB access:
fixed wire/order, valid nonzero and zero replies, actual lengths and errors
at both stages, initial and post-I/O deadline/cancellation, bounded timeout
and invalid callbacks/output. Both U/E empty VMs passed these checks and
mutual-exclusion/registration/unload checks without profile inputs. The
current Clang W=1 hardware build passed, including the updated external
modules; no module-build warnings were reported. The generic new-file
MAINTAINERS warning is covered by the existing tools/testing/ws73/ entry.

Actual allocated U 1-1.1.3 and E 1-2.1.4 each passed the supervisor directly.
An ordinary observation owner first queried identity/version; PID 1 then
unbound that guest owner and explicitly bound the read-only trial. Its new
owner independently queried identity/version again and completed the fixed
register read. Both returned an actual successful four-byte zero response.
Zero is the observed register value, not a substituted value after an error.
Each run checked nine bulk OUTs, five complete successful bulk INs, two
owner drains and VM exit zero. Complete binary/text capture cross-checks
cover all 672 TX bytes and all reply bytes, including the second owner's
identity/version and the fixed register response. No register data was
written, no reset or BLE enable was sent and no firmware was downloaded.

The two IRQ reads per run were cancelled cleanly at guest unbind/final
removal; no successful notification appeared during these bounded trials.
The same selected device was released unowned in runtime. Only its explicit
hostport was attached; four QOM reset/detach options were false and the QEMU
trace had one successful open, one close and no usb_host_reset. No hub,
physical controller or other thread device was operated on.

The first U capture remains FAIL because the supervisor's broad functional
log rejection also matched the pure guard-test messages. Its complete wire
was separately replayed after permitting only the exact three expected pure
check markers; missing/duplicate checks and real startup logs still reject.
The later U trial and E trial below passed directly. Fifty-nine negative
verifier cases passed, including fixed-address/READ-bit/reserved-byte changes,
extra register traffic, short/error/altered captures, second-owner chip/version
mismatches, absent guard evidence and unexpected functional startup.

U: runtime-observe-20261024T062112.588566Z/manifest.json, SHA-256::

  bf3802f44e0e9972063645f440a4a517a8303d1ca145881dd9e4c95646161dc3

E: runtime-observe-20261024T062035.324821Z/manifest.json, SHA-256::

  3a510c8dbec76121f552bb7ebd5cf63c28fedd56f1ce7df643921f6837e00ee4

Build: hardware-hci-build-20261024T061336.600967Z. Empty observation VMs:
runtime-observe-20261024T061636.117762Z (U), and
runtime-observe-20261024T061809.813668Z (E). Raw records remain private.
This proves the fixed read path on these two SDK110 runtime samples only.
A reset write, its completion/disconnect semantics, state clearing and a new
boot/runtime startup have not been implemented or qualified by this result.
No cold, natural recovery, functional warm recovery, RF/peer, address/board,
multi-device or formal upstream gate is closed. This separate test increment
does not grant hardware qualification to the frozen RFC review source.

The unchanged default U/E native-HCI callers also passed separate empty-VM
compatibility checks on this build. A prior no-hardware compatibility attempt
remains FAIL: this agent edited RST while its VM was live, and the final
clean-worktree guard correctly rejected the run. The sections were retained
privately and temporarily removed; sequential reruns then passed on clean
4e0a63b50155. This was a supervisor/source-integrity rejection, not a device
or HCI initialization failure, and no physical device was attached.


Controlled per-device reset and separate U/E boot evidence (2026-10-24 SGT)
===========================================================================

Source 7aa4105b4de91f07b82d1c6f0328ae6bb9ccb1d1 introduces an explicit device_reset=1 mode in the
existing tagged-VM trial. This mode is mutually exclusive with read-only inspection and native HCI startup.
It retains exact DMI/package/serial/enumeration/PID 1 admission, never starts
or publishes a function and requires host-side actual controller-close
provenance plus hash-pinned subsequent physical uses of that same target.
Unknown later use or a consumed reset route rejects admission, including
replayed old runtime snapshots. It is not an automatic fault-recovery path.

Fresh HCC package/USB and SDK110 firmware/ROM checks precede a successful
fixed-register read. Only then can one preserved-value bit 4 update of
0x4001f118 be attempted through the device's register endpoints. An already
set reset bit rejects further writes. All stages share an absolute deadline,
each transfer is bounded, and no retry, fallback value or generic address/
write interface is added. Progress separately records valid READ, attempted
data write, raw USB result and returned status. The old owner becomes
terminal even after a local successful write. USB completion never proves
reset, state clearing or a new usable function instance.

The current hardware build passed Clang W=1. U/E empty VMs executed the
existing 24 read cases and 32 additional attempt cases without USB access.
These cover value preservation, no write after failed reads, each stage's
length/error/deadline/cancellation, already-set bits, invalid input and the
ambiguous data-write/disconnect boundary. Twenty closed-state admission
checks, 22 existing allocation checks and six CLI rejections passed.
Eighty capture negative controls passed, with U/E positive transcripts and
an explicitly ambiguous write followed by independent new boot evidence.
They are offline verifier tests, not extra physical resets.

Allocated U 1-1.1.3 had exactly one physical fixed update, from observed zero
to submitted 0x10, with a successful four-byte USB completion. Its runtime
identity/version replies, all 692 submitted TX bytes, two old-owner drains
and new physical boot descriptors were verified. The original run remains
FAIL: the new guest interface was authorized but not explicitly probed, so
no boot HCC exchange occurred in that VM. Linux interface authorization
requires manual probing, as documented in Documentation/usb/authorization.rst.

No second U reset was sent. A separate VM on its already boot-mode device
then explicitly bound the ordinary WS73 observer and obtained actual READM
U/USB and BOOTVER001 replies, with complete binary/text agreement, one drain
and exit zero. That original manifest also remains FAIL: the old verifier
counted the version twice because the capture serial contains a /dev/kmsg
copy. Its separately attributed replay now feeds text USB events plus one
independent kernel log and validates the complete actual query. It does not
rewrite either U FAIL or claim one continuous successful U reset sequence.

Allocated E 1-2.1.4 subsequently passed the corrected complete supervisor.
It independently returned E/USB and the SDK110 versions, read zero, submitted
one 0x10 update and completed that four-byte USB transfer. A new physical
boot instance was observed on the same hostport, followed by controlled QMP
reattachment to emulated guest port 2, explicit authorization/binding and
fresh HCC E/USB plus BOOTVER001 replies. The two new boot queries add two
OUTs/two complete INs to the 11 runtime OUTs/five complete runtime INs.
Three owner drains and VM exit zero passed. The complete 692 runtime TX
bytes and all runtime/boot reply bytes are checked; no firmware or profile
was supplied and no BLE function was started in either reset attempt.

Each attempted target used only its fixed hostport, eight false per-object
QOM reset/detach checks, two QEMU opens/two closes and no usb_host_reset.
Cached enumeration/descriptors for the other six device ports and two hubs
were unchanged after both attempts. All four allocated devices were unowned
after exit; no hub, physical controller or other-thread device was operated
on. This cached comparison does not establish continuing peer business or
RF isolation. Both one-attempt runtime admissions are now consumed.

U reset attempt, retained FAIL: runtime-observe-20261024T065314.573290Z,
manifest SHA-256::

  bed6251b1edce23fadcf72c3d3843d04270071cedfc3205cd8bd5c4a5abed2c4

U independent boot query, retained verifier FAIL with separately validated
wire evidence: runtime-observe-20261024T065914.666068Z, manifest SHA-256::

  e8d65abd6a79c1eb8d6cdb5e724645423ba2c99bf71bfba0ef3a72a15ac9cb45

E complete controlled attempt/new-boot observation:
runtime-observe-20261024T070321.488144Z, manifest SHA-256::

  a8623f0ceaab7b1d49618ddce4ad0b2d73f10d265927ffacddfa8b7b1134d2a8

Actual build: hardware-hci-build-20261024T063610.197407Z. These experiments
qualify only the stated fixed attempt and bounded new-ROM observations.
They do not prove pristine RAM, cold boot, natural recovery, normal-parent
automatic recovery or a usable restarted BLE/Wi-Fi function. Before function
use, each target still needs the full selected-board three-original-file
load, new runtime identity/version and genuine startup acknowledgments.
No board/address, RF/peer, repeated-start/long-run, formal U/E or frozen RFC
review-source qualification is inherited. Raw evidence remains private.


U/E post-reset three-file load and native HCI (2026-10-24 SGT)
==============================================================

The existing full-load VM supervisor now has a disjoint --post-reset
admission for the two specifically reviewed boot samples. It does not reuse
an initial-ROM admission or a user power-cycle confirmation. It rechecks
all pinned reset/boot manifests and original materials, the actual wire
parsers, same serial/topology/boot epoch and later physical-use history.
Both host preparation and the immediate pre-attachment check must succeed.
Any later unreviewed use or consumed trial rejects admission, including an
old boot snapshot. No new reset is performed by this loading mode.

The U route retains both original reset/boot verifier FAILs and independently
validates only their actual established subsets. It never promotes them to
one continuous successful reset trial. E uses its direct complete controlled
reset/new-boot evidence. Neither route attests power removal, pristine RAM,
cold automatic selection or natural recovery. Twenty-two offline admission
cases and seven pre-VM CLI rejections passed; the previous allocation/reset
checks remained at 22/20. U/E empty VMs passed the original loader/profile/
native-HCI caller checks. The physical image excludes KUnit, dummy_hcd,
gadget and synthetic HCI/USB test configurations.

Both actual full trials use image source
7aa4105b4de91f07b82d1c6f0328ae6bb9ccb1d1. Their clean worktree head was
df83956f8afb, with only the documented RST delta since that build. No kernel
change or rebuild was needed. U 1-1.1.3 and E 1-2.1.4 each freshly reported
the expected boot HCC identity before all three exact SDK110 inputs were
validated and the existing preparation/download path ran. U used only the
US ws73.bin, wifi_cali.bin and btc_cali.bin; E used its separate E originals.
No combined image, shared global firmware fallback or cross-model retry was
used. Each new runtime owner again matched package/USB and SDK110 versions,
then completed real platform/customization/BLE startup and disable before
publishing the actual native HCI consumer. No BSP_READY or Complete was
invented from the previous instance.

U loaded 172580 payload bytes in seven FILES blocks after 11 acknowledged
preparation writes; its boot checks cover 29 OUTs and 28 replies. Native HCI
completed 23 real command/reply pairs, management Read Info, acknowledged
native auto-off, commanded removal and three callback drains. E loaded
192632 payload bytes in eight blocks, with 11 preparation acknowledgments,
31 boot OUTs and 30 replies; its native sequence has 24 real command/reply
pairs and the same completion/removal/drain checks. Both VMs exited zero.
Each controller's management address matched its earlier initial sample
result and the two samples' addresses remain different. This is observed
stability across these software-reset experiments, not factory provisioning,
public-address origin or global uniqueness proof. No raw address is public.

Each run used only its explicit hostport, two boot/runtime epochs, eight
false per-object QOM reset/detach options, two QEMU opens/two closes and no
usb_host_reset. Only the two intended targets' cached USB identities changed
to runtime; the other six known device ports and both hubs retained their
cached enumeration/descriptors. All four allocated ports were unowned after
exit and no project VM remained. This is not active peer-business isolation.
Both post-reset loading admissions are consumed; their old boot snapshots
now reject. The earlier U reset/boot FAILs remain unchanged.

U actual full load: load-hci-trial-20261024T073347.664989Z/manifest.json,
SHA-256::

  b55f95883f96b6c10751b477ef788afa5840c97acd2adf8ef6957bd36b3efc1f

E actual full load: load-hci-trial-20261024T073645.989617Z/manifest.json,
SHA-256::

  48a4448f8c8a773b1fab2d8836eb40ac9905b9f9cef38ab21f4f4b5e4b3aa383

The established capture limits remain: boot TX prefixes plus exact actual
lengths/ACKs, not every on-wire firmware byte; original full inputs are
hash-pinned. Runtime RX/HCI is complete, while the final 352 customization
TX padding bytes are uncaptured. Firmware-bearing records remain private.
This proves one per-model complete native initialization after accurately
identified post-software-reset boot, under the explicit reference profile.
It does not qualify cold/natural recovery, normal-parent automatic loading,
RF/peer/GATT, board/address policy, repeated starts/function cycles/long-run,
formal joint support or the separate frozen RFC review tree. Repeated native
power cycles should retain a live qualified owner where possible; later
whole-device resets need new actual closed-controller evidence, not reuse
of these consumed admissions.


U/E actual native power cycles (2026-10-24 SGT)
=================================================

The existing full-load supervisor gained bounded --power-cycles (0..100).
It retains one genuinely initialized shared owner, uses native MGMT Set
Powered, and requires the actual ordered BLE_CLOSE reply before each next
power-on. MGMT powered-off alone is insufficient: native close work can
still be pending. Each enabled interval must contain the native information/
initialization command set, successful actual HCI replies and HCI Reset.
There are no fabricated completions or private command-credit accounting.
The input UART is separate from the established file-backed capture UART.
The guest validates its input channel before any hardware attach, rejects
wrong ordinals, repeated/pending commands and malformed settings replies,
and has a finite absolute deadline. Unknown or missing capture data fails.

Two actual allocated samples each completed 100 additional native on/off
cycles after their own HCC-identified, same-board SDK110 three-original-file
load and fresh runtime identity/startup. The image source remains
7aa4105b4de91f07b82d1c6f0328ae6bb9ccb1d1; the clean trial head was
ea19bc0647c8, whose intervening changes are documentation only. This is not
hardware qualification of the independent frozen RFC review tree.

* U 1-1.1.3: 2,423 actual HCI commands and 2,423 successful matching replies;
  the initial open has 23 and each subsequent open 24 commands.
* E 1-2.1.4: 2,524 actual HCI commands and 2,524 successful matching replies;
  the initial open has 24 and each subsequent open 25 commands.

Each has 204 actual class action requests, 102 ordered close replies,
307 function notifications, 200 successful native management power command
completions, one configured HCI index, explicit final index removal, three
owner drains and VM exit zero. The 100 cycles are function cycles on a
retained owner, not 100 downloads, device resets or cold starts. Native
address resolution enable and extended advertising disable are normal
power-on configuration; the latter's parameters are constrained to disable
only. No discovery, advertising enable, connection or peer traffic was used.

Failures are retained with their original manifests. U's final supervisor
initially rejected standard power-on opcodes absent from its old whitelist.
After checking their kernel definitions and restricting their parameters,
independent replay of all complete command/reply, management, close and
text/binary captures verifies the actual 100 cycles. This does not rewrite
that original FAIL or represent a second physical run. E's final supervisor
directly passed all checks. An earlier E trial stopped before any requested
cycle because independent text/binary event counts disagreed; its missing
HCI capture is not promoted to PASS. Only its completely paired close control
path was used to establish the next individually reviewed reset admission.
Earlier E reset-time old-IN EPROTO and partial-console-line verifier FAILs
also remain. The latter reset's actual write and independent new-boot HCC
are separate evidence; no write was retried from the consumed state.

Offline checks include 22 allocation, 20 closed-state and 22 boot-admission
cases, 83 runtime capture rejections, native management parser checks,
100-cycle U/E transcript negative controls and six actual runner fragmented-
line/error tests. Empty-VM input-channel preflight passed. These counts are
not additional hardware cycles. All reset/loading admissions are consumed.
Both full trials have two explicit same-target QEMU boot/runtime opens and
closes, eight false per-object reset/detach options and no usb_host_reset.
After all VMs exited, all four allocated ports were unowned, leases and the
build lock were available, and the other six cached device ports and both
hubs were unchanged. This is not continuing peer-business isolation.

U actual capture: load-hci-trial-20261024T082151.475467Z/manifest.json,
original FAIL retained; SHA-256::

  c6ed1711e09fe8e6bcdb6bc43ea339215c0e01a90c67fc0135442503cbc7e3a5

E actual capture: load-hci-trial-20261024T082921.230608Z/manifest.json,
final supervisor PASS; SHA-256::

  5ad1699c6f259a2d1f09066039d13e5919027b4ec1652baa950d5bb40a353431

Separate assessment: native-power-cycles-20261024/completion.json,
SHA-256::

  73e6905abd2a7e55f67331b5ea845f057bb81c4fb7cd5e7dc1e18b9d460e856a

Firmware-bearing raw evidence remains private. Boot TX has prefixes and
exact actual lengths/ACKs with full original input hashes; runtime RX/HCI is
complete, with the previously documented final 352 customization TX padding
bytes uncaptured. The explicit reference board profile and address source
remain unqualified. This meets the 100 native function-cycle count for these
two samples only. Twenty starts per model, 24-hour real business, BlueZ RF/
independent peer/GATT/security, board/address policy, Wi-Fi, cold selection,
natural/automatic recovery and formal joint U/E support remain open.


E actual BlueZ discovery/advertising control (2026-10-24 SGT)
=============================================================

The existing full-load runner gained an optional bounded --bluez-smoke path.
It keeps the qualified live shared owner, uses native MGMT power and the
installed, hash-pinned BlueZ btmgmt binary plus exact dependencies, and runs
Read Info, LE discovery start/stop and non-connectable advertising start/stop.
The guest permits only fixed argv and ordered actions, with at most two own
children for concurrent discovery/stop, bounded output, parent/VM deadlines
and explicit child reaping. A separate input UART leaves the established
capture UART unchanged. Empty VMs validate an actual BlueZ MGMT revision
request/callback and exit without attaching a USB device.

On allocated E 1-2.1.4, the actual sequence completed both stop operations,
a native power-off, the actual BLE_CLOSE reply, explicit final HCI removal,
three owner drains and VM exit zero. There are 61 actual successful HCI
command/reply pairs, six BLE class action transfers and three ordered close
replies. Discovery was enabled for 3,130,168 microseconds and advertising
for 3,080,992 microseconds between captured enable/disable submissions.
The H4 stream contains 35 format-valid advertising report entries; these
are not 35 distinct devices or a controlled peer result. BlueZ discovery
and advertising settings events agree with the successful HCI operations.
No connection, pairing or GATT request was sent to a discovered address.
Raw discovered addresses and advertising contents remain private.

The image/module source remains 7aa4105b4de9, with clean trial worktree
3792da36e06d (only documentation differences). The pinned installed binary
is btmgmt 5.87, package bluez-5.87+1.git8750129efca8-1.fc44.x86_64,
SHA-256 d878b1f0fad64006de98125cbce33331942d70899f42af110320d63c686c85be.
E used its own SDK110 three originals, fresh boot E/USB and runtime
package/USB/version verification and genuine startup, with the explicitly
unqualified reference board profile. No other-model or combined input was
present. Two explicit boot/runtime QEMU opens and closes, eight false
per-object reset/detach options and no usb_host_reset were verified.

E's original final supervisor FAIL is retained. Its old radio checker
accepted resolvable/static random addresses but omitted the non-resolvable
private address that the native kernel generates for active scanning.
The corrected checker accepts top bits 00, still rejects reserved 10 and
zero/ones values, and verifies every command/event, management/tool output,
complete text/binary capture, actual close, loader and trace independently.
No physical trial was repeated for this replay. Synthetic U/E radio tests,
parameter/report mutations, existing native-cycle guards, fixed-child guard
checks and fragmented-line checks are offline evidence, not extra RF runs.

An earlier U 1-1.1.3 trial completed native power-on and BlueZ Read Info but
stopped before any scanning/advertising command. Its original timeout FAIL
is retained. BlueZ's positive --timeout keeps a noninteractive command alive
until that timer; the supervisor used the same wait interval. Removing that
option while retaining the independent parent/VM deadline fixed the tool
lifecycle, as verified in the later empty VM and actual E trial. U has no
proven final BLE_CLOSE ACK after that abort. It is unowned at USB level but
must not receive another reset/load admission or fabricated disabled/startup
state. The next U step needs a narrowly scoped, VM-only actual BLE quiescence
operation and reply, then a new reviewed reset/boot/load and radio trial.

E original capture: load-hci-trial-20261024T090635.660906Z/manifest.json,
original FAIL retained, separately replay-verified; SHA-256::

  241e41f5439e6b7a0a003b2b59fae44413e65907531fc155767ad20418e68a7c

U pre-radio capture: load-hci-trial-20261024T085900.327087Z/manifest.json,
original FAIL retained; SHA-256::

  fe6ea12ce3c08abfdf6f7fcc0db9e049a378ed9250574f723907111cced60652

Separate assessment: bluez-radio-smoke-20261024/completion.json,
SHA-256::

  e808baafeaef13d196a8658b47f28b740ebbf45071a5cf37905d9389e55f42c7

After exit all four allocated USB nodes were unowned, no VM remained and
port leases were available. Only intended U/E enumerations changed; the
other six known device ports and both hubs retained cached descriptors and
numbers. That does not attest active peer-business isolation or U functional
closure. Firmware-bearing raw records remain private. Established boot TX
prefix/exact-length/ACK and final 352 customization TX padding limits remain;
runtime RX/HCI is complete. Controller command acknowledgments and ambient
reports do not independently verify transmitted advertising, controlled peer
interop, connection/GATT/security, address/board provenance, cold/natural
recovery, Wi-Fi, long-duration business, frozen RFC hardware qualification
or formal U/E support. Those gates remain open.


Terminal BLE quiescence and new-source U/E BlueZ (2026-10-24 SGT)
=================================================================

The shared control layer now provides an explicit stop-only quiescence
operation for a fresh UNKNOWN object. Ordinary unknown-to-disable remains
rejected. Quiescence issues only command 26 and requires both the complete
four-byte local transfer and matching actual close reply within one bounded
deadline. Success enters terminal QUIESCED, never DISABLED/ready; repeated
quiescence, enable and disable cannot reuse it. Active/stable normal control
objects are rejected without another transfer. TX, wrong/duplicate/late
reply, cancellation and timeout failures retain the first terminal error.
An RX reply may legitimately precede local EP0 status completion, but cannot
hide a failed/short transfer. No HCI completion is synthesized.

The exclusive owner must verify firmware identity, absence of live consumers
and no earlier ambiguous stop matcher before using this operation. Its
actual consumer is the existing tagged-VM trial's disjoint quiesce_ble=1
mode: fresh live U/E package/USB and SDK110 firmware/ROM verification,
no firmware/profile input or startup arm, one stop/ACK, then terminal owner
and drains. It cannot combine with native_hci, inspect_reset or device_reset.
It cannot publish a child or infer BSP_READY/startup DONE. Later reset/loading
needs another independently source-bound admission; the quiesced object is
not reused. There is no generic register/control or new user ABI.

Implementation is signed commit 627795f9db19. The source actually built and
run below is its descendant c2f291611a2f4ca83519c17f65a1bda2005c8214, which
also fixes the KUnit runner's append-open of the shared build lock before
nonblocking admission. The first pre-lock KUnit attempt was rejected before
any build/test; no lock owner was interrupted or identity inferred after
fuser found no remaining holder. The later clean-source runs passed 107/107
each: KASAN/UBSAN/lockdep/DEBUG_ATOMIC_SLEEP and independent strict KCSAN/
UBSAN. The KCSAN early-enable and all three selftests were verified; that is
sampled evidence, not a race-freedom or new formal-model proof. Four new
control cases cover terminal lifecycle, TX/reply failures, admission and
independent-thread RX. The hardware kernel/modules build has no W=1 warning.

An initial empty quiescence VM failed its DMI product guard before any
physical attachment because the new caller omitted board tagging. That FAIL
is retained. Corrected U/E empty VMs directly passed, with contradictory
mode rejection, no HCI publication and no firmware input. Offline checks
include 24 malformed/missing/duplicate stop-capture rejections, 12 immutable
prior-state admissions, U/E separation, reply-before-local-completion and
four pre-VM CLI rejections. All five stale quiesce/reset/load routes reject
replay. These are not additional hardware successes.

Allocated U 1-1.1.3 actually passed the stop-only trial: two independently
queried matching live identities/versions, eight complete platform bulk OUTs
(656 bytes), four platform replies plus one complete close notification,
one successful class stop and two owner drains. No function enable, profile,
register operation, reset or HCI publication occurred. This replaces the
unconfirmed physical close after the earlier BlueZ info timeout with new
actual evidence; the original FAIL itself remains unchanged.

That actual U close and E's separately verified previous radio-close evidence
then admitted one controlled per-device software reset each, fresh boot HCC
and a new complete same-model SDK110 three-original-file startup. Both models
now directly pass the corrected BlueZ supervisor on the same new source:

* U 1-1.1.3: 59 real successful HCI command/reply pairs, 20 valid report entries.
* E 1-2.1.4: 61 real successful HCI command/reply pairs, 32 valid report entries.

Both pass native initialization, actual BlueZ Read Info and bounded LE
discovery/advertising start/stop, management settings, six class actions,
three ordered close replies, explicit final HCI removal, three owner drains
and VM exit zero. Advertising remains non-connectable. Report entries are
ambient observations, not distinct-device counts or controlled-peer tests.
No prior-source E result is inherited for these new-source assertions.
Each full trial has two explicit same-target boot/runtime QEMU opens and
closes, eight false reset/detach options and no usb_host_reset. No device or
hub was manually replugged, port-powered or passed through as a controller.

Actual U quiescence: runtime-observe-20261024T095207.589165Z/manifest.json,
SHA-256::

  7f573858167b402eb82893c6281e59258447c7ef4a23a6a20fd0f216ff1fc139

New-source U BlueZ: load-hci-trial-20261024T095934.441113Z/manifest.json,
SHA-256::

  623ff0d1a5859030394e1b864b0dd0697fabce8224f93c56204eabf122fefd4f

New-source E BlueZ: load-hci-trial-20261024T100418.319095Z/manifest.json,
SHA-256::

  a9a9a4aca3abc790b99f0fc54c0d34389c2334cd220195ad5f1d2baec80b2f23

Independent exact-source assessment: ble-quiesce-20261024/completion.json,
SHA-256::

  339da99bd0237e20d73ccda5764e39b0accaf16d02fdd22fa3557ca4ce51ddc3

After the hardware VMs exited, all four allocated nodes were unowned, port
leases/build lock were available, and only the intended U/E enumeration
numbers changed. The other six known device ports and both hubs retained
cached identities/descriptors; continuing peer-business isolation is not
inferred. Firmware-bearing and discovered-address/data captures stay private.
The same boot TX prefix/exact-length/ACK and final 352 customization TX
padding limits remain; runtime RX/HCI is complete. Board/address provenance,
cold automatic selection, controlled independent-peer connection/GATT/
security, Wi-Fi, 20 starts/24-hour business, PM, natural/automatic recovery,
frozen RFC hardware qualification and formal joint support remain open.
The normal parent is still observation-only. Next development should use
controlled allocated peers with actual bilateral payload checks, while
keeping same-family development evidence distinct from independent interop.


Parallel U/E native BlueZ VM isolation (2026-10-24 SGT)
=======================================================

The existing private single-device supervisor now separates exclusive-lock
image preparation from shared-lock execution of immutable per-port inputs.
A prepared execution requires an explicit reviewed manifest SHA-256, exact
source/build/tool contracts, allocation/port/serial/board and mode, and the
selected SDK110 three originals. It performs no compilation. Each invocation
has its own private QMP/control socket directory, shared hardware lease and
exclusive per-port lease. Both boot and runtime attachments still recheck
live identity, descriptors, topology, ownership and all four reset/detach
options. No enumerated-number selector or global mixed-board image is used.
The default prepare-and-run route retains exclusive global locking.

Twenty-five production admission mutations per model reject changed inputs,
including rehashed executable, firmware-role, serial/board and build-proof
changes; eight actual CLI/lock conflicts reject before VM/USB access or trial
record creation. Two actual empty VMs overlapped for 2.209 seconds, with
separate sockets and no physical attachment. An exclusive build-lock probe
was rejected while their shared runtime locks were held. These checks do
not constitute hardware or concurrency-freedom qualification.

Fresh reviewed close epochs admitted one fixed per-device internal reset
for each selected sample. U's reset/new boot observation directly passed.
E's original final verifier result remains FAIL: one matched empty runtime
IRQ completion returned -EPROTO after the fixed reset data submission. Its
new boot HCC identity/version and complete transfer record were already
captured. A separately recorded full replay accepts this one disappearing
runtime IRQ error only after the validated fixed write and with independent
new-boot proof. Other errors, earlier completions, wrong endpoints, nonempty
payloads and missing capture/boot remain rejected; ordinary observation
retains its strict error policy. Ninety parser controls pass. This is a
bounded explanation of that actual reset, not natural recovery or a rewrite
of the failed attempt. An earlier E invocation was rejected by the global
nonblocking build lock before any physical attempt; it is retained too.

Both selected devices then directly passed a simultaneous real trial on
source c2f291611a2f4ca83519c17f65a1bda2005c8214. Each separate VM used its
own explicit U/E SDK110 code and two calibration originals, fresh boot HCC,
complete load/controlled runtime handoff, runtime identity/version and genuine
BLE startup/disable before native HCI. Their QMP lifetimes overlapped for
16.541 seconds. Two VMs used four total vCPUs and zero compilation jobs.
Actual pinned BlueZ discovery and non-connectable advertising control passed:

* U 1-1.1.3: 59 successful HCI command/reply pairs, 24 valid report entries.
* E 1-2.1.4: 61 successful HCI command/reply pairs, 30 valid report entries.

Each full trial has six class actions, three matching close replies, final
HCI removal, three owner drains and exit zero. Each has two intended physical
boot/runtime opens/closes, eight actual false QOM reset/detach settings and
no usb_host_reset. The archived initramfs was independently parsed: its only
firmware inputs are the selected model's three originals and the separately
recorded reference customization body. Original hashes and the full existing
boot, runtime binary/text, HCI/BlueZ and close verifiers were replayed.

U: load-hci-trial-20261024T110030.491730Z/manifest.json, SHA-256::

  3567afb03074c7c0d72caa434c9c870a1255d7ac0a37f8d619a29acc0c3f2de0

E: load-hci-trial-20261024T110030.491253Z/manifest.json, SHA-256::

  657870bd35bb152c0142a4e7a5c86703e0b819efbb4320121bce5d53e4a59fa5

Assessment: peer-runner-preparation-20261024T105204.887731Z/completion.json,
SHA-256::

  2b347b6ec4bad96ab0c76be1bd25bc0b4e5ae5d6cc82810857679274cc66935e

All four allocated nodes were unowned after exit and all five hardware
leases available. Only the intended U/E enumeration numbers changed; the
other six device ports and both hubs retained cached identities/descriptors.
An unrelated WS53 virtual test with no USB passthrough was observed and left
running. Active other-device business isolation is not established by this
snapshot. Firmware-bearing records and discovered addresses remain private.

These are separate-model overlapping VM and real controller-control results,
not controlled peer discovery, verified advertising emission, connection,
ACL/GATT/security, role exchange or independent interoperability. No ambient
discovered device was connected. Normal automatic loading, cold automatic
selection, board/address provenance, natural/automatic recovery, PM, long-run
business, Wi-Fi and formal U/E/upstream gates remain open. Prior KUnit and
100-cycle results keep their own source/scope; no new kernel behavior was
introduced by this supervisor increment. Next peer work must establish fresh
admissions and retain genuine qualified owners, identify only the allocated
peer by a controlled session, and verify real bilateral payload and teardown.

Controlled peer attempts and native advertising handle mapping (2026-10-24 SGT)
----------------------------------------------------------------------------

Six allocated U/E physical attempts were retained as FAIL across three source
epochs. They used the existing per-port SDK110 three-file preparation and
reset-free QEMU path, with independent images, identities, port leases and
evidence. No ambient advertiser was connected. Session UUIDs identify only the
allocated development peer; they do not authenticate it or establish independent
interoperability.

The first pair exposed a native Bluetooth handle mapping error. A logical
advertising instance may have controller handle zero on a one-set controller.
Advertising parameters and data already used that mapping, but Set Advertising
Set Random Address and Remove Advertising Set used the logical instance. The
actual E returned status 0x42 for opcode 0x2035; the corresponding U kernel
error was also retained, with its incomplete HCI capture explicitly unqualified.
Signed changes ``f076f17bd49b`` and ``9c03e516105d`` map those transmit paths.
Signed change ``6a11e11cce8b`` adds lookup by controller handle for completion
and termination callbacks, retaining logical IDs for management notifications
and connection restart. Mapped handle zero must update its actual instance,
rather than the unrelated default advertising address or enabled state.
Periodic enable receive mapping is corrected, but periodic advertising is not
qualified; remaining periodic transmit paths require a separate audit.

Pure checks compile the actual production helpers and callbacks with bounded
operation stubs under Clang UBSAN: 496 transmit contracts and 36 callback
contracts pass, and the pre-fix failures are reproduced. These checks do not
prove in-kernel scheduling, locking or memory lifetime. The combined 107-case
KASAN/UBSAN and strict KCSAN/UBSAN VM campaigns pass on clean
``6b3d8b4dd4fa980610aadff8dd79b37bfee2d3f4``:

* ``20261024T141209.090105Z``; manifest SHA-256
  ``787bfcfa153c109e2cd354432be2241754ccc634b64d2e8cbd8845686b6307f3``.
* ``20261024T141345.200821Z``; manifest SHA-256
  ``3681407c59545a8d4734cfdbeca6eef5c23230b2bb27373e977540a219c233bc``.

The second E attempt stopped at the complete-capture gate before native power
on: five text RX records were absent although every binary RX was at most
188 bytes. Thus the 512-byte capture ceiling was not the explanation. The
original attempt did not record the text monitor loss counter, so its loss
mechanism is not proven. PID 1 previously read usbmon while also performing
sleepable driver initialization. The existing runner now forks a dedicated
bounded capture process, uses a separate virtio serial port, assembles complete
text lines, and checks the actual kernel text loss counter and binary loss
counter. It requires clean drain, explicit finish and child reap before a new
successful result can be produced. The finish command admits only an empty VM
or actual management index removal; it sends no device/function command.

The HCI-only fixture enables the virtio menu, PCI transport and console. A
missing-menu configuration failure was retained before the fix; no VM or USB
operation used that rejected configuration. Clang W=1 kernel/module builds
pass on clean ``09720a346471ab848671ee00fc4ae6c81f4c4197``; only the test
builder changed since the above KUnit source. Build record
``hardware-hci-build-20261024T141557.910002Z`` has manifest SHA-256
``2b9ea1a724aeaad2d4b14cb12eb76adba6e37abf8ef94997432a8822c8d8398f``.
U/E empty VMs validate independent capture start, zero text loss and clean
finish/reap. Pure tests cover framing, loss counters and finish rejection;
the finish tests trap all host access and signal operations. Two historical
physical captures replay exactly, without inheriting the new capture scope.

No manual replug was required for the next physical work. A separately
captured U stop ACK, then fixed per-device U/E resets and new boot HCC
observations established fresh, non-cold admissions. These are not USB reset
ioctls, hub/controller operations, power removal, pristine-RAM evidence or
natural recovery. Old consumed admissions were not reused. The final pair
uses the above ``09720a346471`` image:

* U ``load-hci-trial-20261024T142339.641825Z`` remains FAIL; manifest SHA-256
  ``35ff20d4aa000fa406b6f4c31e42dcfcdb2ef1c34588cee7e05c877d2589d66c``.
  Opcode 0x2035 now uses handle zero and succeeds, but extended advertising
  enable 0x2039 returns status 0x12, reported by the native stack as -22.
  Its 60 binary/text runtime RX records match, with no recorded text loss;
  the failed guest powers down without clean collector finish, so complete
  capture shutdown is not qualified. A subsequent fresh stop-only VM obtains
  a real ACK and drains the owner; it does not make the failed trial ready.
* E ``load-hci-trial-20261024T142339.640175Z`` remains FAIL because no allocated
  session peer is seen; manifest SHA-256
  ``40f9a3d5ce0be233b2714ee645e7d59e0e88699292155c4c7379132bb48be0ea``.
  All six BlueZ child actions succeed. The unmodified verifier reaches the
  peer-presence gate after checking 60 actual successful command/reply pairs,
  112 complete binary/text runtime RX records, six class transfers, three
  close replies and three drains. There are 40 ambient reports, no controlled
  peer report, and no kernel diagnostic. Remove Advertising Set uses wire
  handle zero while BlueZ receives removal of logical instance one. The
  independent collector records zero text loss and clean finish/reap. The
  original FAIL was reproduced, not overridden to manufacture a peer PASS.

The repeated U 0x2039 failure is unresolved. Earlier successful default
advertising followed scanning and global random-address initialization;
the failing session-advertising path differs. That correlation is not proof
of a controller quirk, valid workaround or root cause. No fake completion,
global-address workaround, legacy fallback or alternate firmware was added.
A separately identified connectable test, suitable for later actual connection
work, must retain this nonconnectable failure and its distinct scope.

Both final trial VMs and the later U stop-only VM exit zero, and the allocated
devices are unowned in the current-user-visible check. Forbidden ports retain
their exact identities and enumeration numbers. The final U stop record is
``runtime-observe-20261024T142613.930563Z``, manifest SHA-256
``7ea102ad978639c2b8fc78a42994690a451bfe8ec511e14ef5060f6fd3a81890``.
The private aggregate ``controlled-peer-discovery-20261024T111442.826206Z``
completion SHA-256 is
``da2a2a834ae854b09ac6acf8c24bedd729508087a9c81b8b0e3f009f90845529``.
Raw firmware, configuration bytes and radio addresses remain private.

Controlled bilateral discovery, connection/ACL/GATT/security and role exchange
are not passed. Independent-peer interoperability, cold automatic selection,
board/address provenance, natural recovery/PM, sustained business, coexistence,
Wi-Fi STA, firmware delivery and formal joint U/E/upstream gates remain open.
The existing roadmap and issues remain authoritative; no RFC mail was sent.

Allocated U/E bilateral connectable discovery (2026-10-24 SGT)
------------------------------------------------------------

The existing private runner now has an explicit ``--peer-connectable`` mode.
It uses stock BlueZ ``btmgmt add-adv -c`` with the allocated session payload,
and verifies legacy connectable/scannable undirected properties, 1M PHY and
public address type on the wire. This is a different declared experiment from
the retained nonconnectable U 0x2039 failure, not a workaround claimed to fix
that failure. The SDK110 example uses public-type connectable undirected
advertising, but its fixed example address was not copied into this test.
The actual native controller address is used and its provenance remains
unqualified. The ordinary/nonconnectable checker continues rejecting
connectable parameters unless this explicit mode is selected.

Two new, independently recorded per-device fixed resets and new boot HCC
observations established fresh admissions; the previous boot tokens were
consumed and not reused. Per-port SDK110 original three-file images and
runtime U/E identity/version checks remain unchanged. No manual power cycle,
hub/controller operation, ambient-device connection or alternate firmware
was used. Both actual VMs run the existing hardware image from
``09720a346471ab848671ee00fc4ae6c81f4c4197``; the worktree at execution is
``cd81c985e93544e14406cb544f12dd4a7c6b4e18`` with only documented changes
since that build.

The allocated U advertises first while E scans; E then advertises while U
scans. Both independently finish all six BlueZ actions, native power off,
three actual close replies, three owner drains and clean independent capture
finish/reap. Each side receives the other side's session UUID and name in
management discovery and in a complete actual HCI advertising report. The
final correlation compares each received peer address/type with the other
side's actual transmitted address/type, and each expected payload hash with
the other's session payload hash. The two observed addresses are distinct
within this trial. This correlation does not establish address provenance,
authentication or general uniqueness.

* U ``load-hci-trial-20261024T144217.325276Z``: 57 successful actual HCI
  command/reply pairs and 95 complete runtime RX records; manifest SHA-256
  ``ae9a479932136f0a818c73b9579c4ad5b3bfb886496d81c9af0199815f4a4106``.
* E ``load-hci-trial-20261024T144217.325534Z``: 59 successful actual HCI
  command/reply pairs and 109 complete runtime RX records; manifest SHA-256
  ``b10d835a60bb604377c4cb62b0922c2c112703794bc94a2858b0487b90f7ef49``.

Both original single-side statuses explicitly required bilateral correlation.
The final result is
``ALLOCATED_CONNECTABLE_BILATERAL_DISCOVERY_CORRELATED`` after the correlation
above, exact replay with each archived verifier and another exact replay with
the current verifier. Twenty correlation mutations are rejected without
editing any original artifact. Both independent collectors record zero
binary/text loss, and all actual runtime RX records match their text
counterparts. No KASAN/UBSAN/lockdep diagnostic is observed. The VMs exit zero;
all allocated ports are unowned in the current-user-visible check, and the
forbidden ports retain their exact identities and enumeration numbers.

For the next native ATT/GATT step, official unmodified BlueZ GATT client/server
sources were obtained at ``8750129efca858ff7e88e0d3166851fdd4590d8e``. Direct
Git and codeload TLS failures are retained. A read-only GitHub file retrieval
at that exact commit obtained the required subset; all 43 Git blob hashes
were checked before building under the existing global lock. No host package,
driver or kernel was installed. A tagged empty VM successfully runs only the
two tools' help paths with their real dynamic libraries. This is dependency
and execution preparation, not an ATT/GATT or physical connection result.
Its record ``load-hci-trial-20261024T150055.177608Z`` has no physical attach;
the original negative-invocation diagnostic mismatch is retained, and five
isolated checks trap all process/device/QEMU entry points while verifying
that the help-probe flag rejects hardware and radio modes. The direct
hardware-flag diagnostic blocked by automatic review was not executed.

The private aggregate ``connectable-peer-20261024T143706.052491Z`` completion
SHA-256 is
``093806db855f53a7d10236731b50726d6d86f6601af0e7ac7b0dda26638c8b55``.
It binds both physical records, bilateral correlation, all setup failures,
standard tool source/build provenance, empty VM evidence and next-step
contract. Raw addresses, configuration and firmware remain private.

Only controlled bilateral discovery is newly established. A connection must
still be proven by actual native connection events, correct roles/addresses
and controlled socket peers; real ACL/L2CAP/ATT payload and GATT readback must
be verified in both directions, followed by orderly disconnect and role
exchange. Local transport/NOCP completion is not remote application receipt.
Pairing, encryption, bond and independent interoperability remain untested.
Both devices use the same firmware family. Connectable/public-type success
does not isolate why nonconnectable/random-type advertising was rejected;
that failure remains open. Cold automatic selection, board/address provenance,
natural recovery/PM, sustained business/coexistence, Wi-Fi STA, firmware
delivery and formal joint U/E/upstream acceptance also remain open.


Native GATT attempts and retained timing failures (2026-10-25 SGT)
-----------------------------------------------------------------

The existing per-device VM runner now invokes the unmodified, pinned BlueZ
GATT tools described above, with U as server and E as client. It requires the
previous exact bilateral discovery record, fresh controller addresses and a
new session UUID before connecting only the allocated peer. The fixed client
sequence would discover the actual GAP Device Name characteristic, write a
64-byte random hex token, read it back, then disconnect. The server is a
bounded RAM-only development service. This is low-security development;
role exchange, pairing, encryption and independent interoperability are not
established. Helper implementations and synthetic vectors are not application
communication evidence.

Five actual paired attempts used allocated U 1-1.1.3 and E 1-2.1.4, each with
its own SDK110 original three-file image and boot/runtime HCC checks. All ten
physical records remain FAIL. The kernel/module source is exactly
``09720a346471ab848671ee00fc4ae6c81f4c4197``; execution worktree HEAD is
``025fc834b7c7e07dfe73167a1c5e0f215e5ff73d`` with only documentation changes
since that build. No new production driver quirk or fabricated capability,
completion or wireless ACK was introduced.

The attempts establish distinct observations, not a combined success:

* Initial short discovery: E did not receive the new U session advertisement;
  no client connection was started.
* Overlong discovery: the test's 12-second scan exceeded the native
  ``DISCOV_LE_TIMEOUT`` of 10240 ms. Stock BlueZ exits when discovery ends;
  both guests therefore stopped before GATT. The test window was corrected
  to eight seconds, without changing the Bluetooth stack timer.
* Eight-second discovery: E submitted Extended Create Connection (0x2043),
  received successful Command Status, and sent Create Connection Cancel
  (0x200e) 20544641 microseconds later. Successful Cancel Complete preceded
  an Enhanced Connection Complete with status 0x02. This is a cancellation
  result, not an immediate unsupported-opcode response. The restarted U
  advertisement was not independently confirmed during this initiation.
* Fresh rediscovery: E received the first U advertisement, but its second
  scan did not receive the restarted U session advertisement despite
  successful advertising command replies. The client was not started.
* First advertisement retained: U listened before enabling its first
  advertising set and kept it active; E received that exact session and
  submitted 0x2043 with successful Command Status. Native connection setup
  included an additional roughly ten-second scan. The test's 22-second
  output deadline ended before the full subsequent initiation timeout;
  no Cancel command or successful connection event was captured.

No successful LE connection or ACL packet is observed in these captures.
In particular, the last experiment cannot establish failure after a complete
native initiation window. Its corrected pending test uses a 42-second client
wait and 55-second server listen wait, with bounded child and VM lifetimes.
Those changes pass the pure guest state/control checks, but their new image
preparation and empty-VM/physical execution have not completed. A first
preparation was rejected by the nonblocking global build lock before creating
an artifact or VM; its retry was not executed because automatic approval
review was unavailable due to an account usage limit. Neither event is a
firmware or hardware result.

All ten original manifest/material hashes were replayed. The explicit boot
stage checks pass, and each captured successful runtime RX record is complete
and matches its text counterpart. This is narrower than clean full-session
capture: some failure exits lack final runtime URB cancellation records, and
none of these failed runs establishes the required clean capture finish/reap.
An initial whole-stream boot replay rejected the undrained runtime records;
that rejection is retained. Subsequent boot-only replay is explicitly scoped
to device 2 and cannot erase that session limitation. Kernel owner drains and
QEMU exit zero are recorded separately. Fresh stop-only VMs obtained actual
BLE close ACKs before any subsequent fixed chip reset; those independent
operations do not turn the failed GATT sessions into orderly disconnects or
natural recovery passes.

.. list-table:: Retained physical GATT attempts
   :header-rows: 1

   * - Model and record
     - Manifest SHA-256
   * - US ``load-hci-trial-20261024T155118.131457Z``
     - ``0cc64dc5e6aaec6ae39eca7e4466059118833c7ae891672d228cf96790f3ef7e``
   * - E ``load-hci-trial-20261024T155118.129356Z``
     - ``b933fa6c5ee50c4f797eab092a5fe6a0f3b091e8ab508a27580d77cb60502bfe``
   * - US ``load-hci-trial-20261024T160252.805805Z``
     - ``3a5d41e04686d0eed9fff8268d2d59ee462fdd8fb30ccf9c7b8e85fad780d088``
   * - E ``load-hci-trial-20261024T160252.805280Z``
     - ``8046abfe9a0014949467e2c319d163991eee8afaa9a0093fe8d61a589c9200d5``
   * - US ``load-hci-trial-20261024T161415.300136Z``
     - ``5645cd56208287156be6438714c29651ab668272537fa3acbd469be38e78bd61``
   * - E ``load-hci-trial-20261024T161415.306654Z``
     - ``ec444dadd6dc5384a9e498942eb4ed3a1b97db70c39e1ea7cfe5bd62c1de9607``
   * - US ``load-hci-trial-20261024T164229.800314Z``
     - ``512b57ec94b078954d76cb65a9847cf4f23786709ef75c805ec6a79a50ae2c11``
   * - E ``load-hci-trial-20261024T164229.804550Z``
     - ``64573ca9a4e3a065bfe26020422e9f136acb9d9c0b86638f038118ddbd97c061``
   * - US ``load-hci-trial-20261024T165328.042864Z``
     - ``0eab011efe2069a3339584ba795f5f6ce04dcbc4353b4247cec535cdb02ddc23``
   * - E ``load-hci-trial-20261024T165328.043785Z``
     - ``e9b8a64cd310cc631019de3531ede070e9a1f8f1f6af106f73bf16feaf606170``

The private checkpoint ``gatt-native-20261024T152056.581201Z/checkpoint.json``
has SHA-256 ``480e0df4fee946b19c31aedc39ba9b42cae971fc1357ed42228ba38520c2474a``.
It binds 220 current materials and 75 referenced manifests, including all
failed hardware attempts, setup failures, source/tool provenance, negative
controls and the corrected pending test. It is a checkpoint of an open
experiment, not a final qualification seal. Firmware, calibration/profile
bytes, raw captures, peer addresses and device serials remain private.

The expanded BlueZ guard checks 38 actual parser/state scenarios; the GATT
child checks two admitted file-open traps, three fixed stdin commands and
17 rejection cases under Clang UBSAN. Existing peer/management/power checks
also pass. ACL reassembly and application-sequence helpers have bounded
synthetic tests, but no actual ACL/ATT sequence exists to qualify. The final
state requires new per-board preparation, empty-VM checks and a complete
native connection window before evaluating a compatibility quirk or claiming
GATT progress. No new manual replug was requested. The other two allocated
samples gain no qualification from these trials.

The prior controlled bilateral discovery result remains valid only in its
original scope. The nonconnectable/random-type U 0x2039 rejection, advertising
restart behavior, address/board provenance, cold automatic selection, natural
recovery, PM, sustained traffic/coexistence, mac80211 STA, firmware delivery
and joint U/E maintainer/mainline gates remain open.
