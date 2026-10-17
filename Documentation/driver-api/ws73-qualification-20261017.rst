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
