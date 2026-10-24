.. SPDX-License-Identifier: GPL-2.0-only

WS73U / WS73E implementation and upstream gates
==============================================

Approved scope
--------------

Implement the USB shared device layer in drivers/misc/ws73, with actual
Bluetooth and Wi-Fi consumers on the auxiliary bus. BLE-only must not depend
on a Wi-Fi Host. The first formal hardware-support series includes both U
and E after independent hardware qualification. Submit shared core plus BLE
first; implement mac80211 STA in parallel and submit it subsequently.
Only USB is in scope. SparkLink/SLE issue 6 remains deferred, open and
non-blocking; no placeholder interfaces or migration are introduced.

Keep SDK 1.10.110 and the separate original code, Wi-Fi calibration and BTC
calibration files. Select firmware/us or firmware/e only after verified HCC
identity; recheck identity at runtime. Combined blobs are not loader inputs.
Do not switch versions or try another package's image on a failed boot.

Execution stages
----------------

The existing GitHub issues in sanchuanhehe/linux-ws73 are authoritative;
issue 1 is the only overall tracker. Dependencies mean required contracts,
not closure of entire preceding issues.

* P0 (1/2/3/12): correct evidence, pin sources and retain all old results.
* P1 (2/3/5/13): establish boot HCC identity and package mapping; specify
  original firmware loading, board setup and runtime verification.
* P2 (4/5/10): shared protocol, USB owner, auxiliary lifecycle, bounded
  transport, firmware loading and reverse-order failure cleanup. Qualify
  virtual failures and then the allocated physical device.
* P3 (7/11/12): native hci_dev, real enable/reply, command/events, BlueZ
  scanning, advertising, connections and peer-verified bidirectional data.
* P4 (4/14): prepare a buildable RFC with the shared layer and actual BLE
  consumer; identify missing E qualification if only U is ready.
* P5 (2/5/7/10/11/12/13/14): qualify BOTH U and E, firmware deployment and
  claimed BLE capabilities before the formal hardware-support series.
* P6 (3/8/11/12): mac80211 STA, scanning, WPA2-CCMP, IP data and reconnect,
  with genuine TX status, RX metadata, security and regulatory behavior.
* P7 (9/10/13/14/15): same-chip coexistence and independent U/E isolation;
  submit the Wi-Fi series after qualifying its actual scope.
* P8 (9--15): PM, natural recovery, performance and ongoing maintenance;
  track maintainer-tree, linux-next and mainline acceptance separately.

Runtime contracts
-----------------

The parent owns USB, identity, firmware, physical epochs, bounded HCC
resources and teardown. Function drivers own protocol state. Shared APIs
are internal, namespaced and driven by real consumers; no private user ABI,
Android Host, global callback registry or duplicated 802.11 stack.

Successful submission transfers buffer ownership to the core; a rejected
submission leaves ownership with the caller. USB completion is not an HCI
Complete or an RF ACK. RX is validated before dispatch. Stop disables new
admission and drains callbacks before release. Never hold the parent state
lock while waiting for function callbacks. Responses need a completion path
independent of the worker waiting for them. Tokenless requests are serial;
a timed-out response must not satisfy a later request or physical epoch.

Initialization proceeds through descriptor validation, boot communication,
verified identity, validation of ALL files, board/calibration preparation,
download, runtime enumeration, identity/version verification, platform
configuration and only then function registration. On failure, stop new
admission, fail waiters, stop producers, cancel/drain URBs and work, remove
children and release references. Boot and runtime USB instances have separate
lifetimes. Do not carry stale callbacks or trusted identity across reconnect.

Evidence corrections and unresolved identity
-------------------------------------------

At pinned SDK d5e68a58fe38c2b04a16769c69de1e8a80e86d74,
driver/platform/hcc/host/hcc_usb_host.h defines DEVICE_BOOT_EP_NUM=2 and
DEVICE_KERNEL_EP_NUM=5. Runtime roles are data IN/OUT, interrupt IN and
register OUT/IN. The old reload function's three-endpoint comment is stale;
the previous development document repeated it and is corrected here.
Endpoint count alone does not establish device identity or compatibility.

plat_firmware.h defines READ_MEG_TIMEOUT=2000 despite a 200ms comment.
hcc_usb_patch_read passes it through to usb_bulk_msg's millisecond timeout.
USB_WAIT_TIME=5 is multiplied by HZ for enumeration completions. Register
operations use RW_REG_TIMEOUT_MS=2000 and at most three attempts. New code
must preserve an absolute deadline rather than extending it per retry.

The runtime GET_CHIP_TYPE query is understood, but its availability before
firmware loading is unproven. Non-UART host_efuse/efuse_opt.c accesses eFuse
through HCC register reads and read-control writes; USB register operations
have boot READM/WRITEM and runtime paths. The mapping from the eFuse chip
byte to U/E is NOT established. Do not equate die ID, chip ID and package.
Only proven read-control addresses/modes may be used; eFuse programming,
locking and arbitrary register access are not part of this driver.
If boot identity cannot be established, fail closed and continue virtual
protocol/transport work; obtain the missing mapping or boot specification.

The user identified libws73-usb as a reference. Its remote HEAD was checked
again and matches the pinned f4d85d0e95af2d0043432e1301fd2f6b660a4906.
src/ws73_usb_firmware.c demonstrates boot READM/WRITEM. In the SDK,
hcc_usb_host_ops.c dispatches hcc_usb_read_reg/write_reg to read_device_reg/
write_device_reg in BUS_USB_BOOT, so boot register access is an explicit
HCC path, not inferred from runtime endpoints. The SDK implementations in
driver/platform/firmware_download/plat_firmware.c return four raw bytes for
READM and use WRITEM OK for a successful write. New code must check exact
transfer lengths; the SDK's positive-length read check is insufficient.

libws73-usb src/ws73_usb_control.c restricts get_chip_id/get_die_id to five
endpoints through id_mode_gate. It reads eFuse byte 0 for chip ID and bytes
1--18 for die ID. Neither function returns the U/S/E package or bus enum.
Its documented observed chip byte 0x18 is not a U/E compatibility table.
SDK g_efuse_cfg also defines TYPE_ID as four bits at bit 168, but no checked
source maps those bits to U/E. Generated eFuse status-register bitfield
definitions referenced by hal_soc_reg.h are absent from this SDK snapshot.
The reference's fixed status polls do not substitute for the SDK's busy,
boot-complete and read-complete conditions. These remain explicit gaps;
no eFuse value or runtime-only call is promoted to verified boot identity.

Subsequent evidence: fixed boot strap register
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The user supplied an existing research report and candidate tool in the
independent SparkLink project. Rechecking the original firmware files here
confirmed that U function 0x408bc2 and E function 0x409896 contain the same
56-byte decoder (SHA-256
f7f1221ea123e5c8f6426f30da777a4b81f8d590e26daa3f9c405659e8379365).
Offsets subtract load address 0x400000 and add the original 64-byte header.
An independent invocation of the Hisi disassembler confirms a read of
0x40019000 followed by this mapping:

.. list-table:: Firmware strap mapping
   :header-rows: 1

   * - Condition
     - Package
     - Bus
   * - v & 0x0c is 0x00 or 0x0c
     - U (0)
     - USB (0)
   * - v & 0x0c is 0x04
     - S (1)
     - SDIO (1)
   * - v & 0x0c is 0x08; v & 0x30 is 0x20
     - E (2)
     - UART (2)
   * - v & 0x0c is 0x08; otherwise
     - E (2)
     - USB (0)

This is now the primary identification candidate; the eFuse route above is
historical investigation, not a prerequisite for using the strap register.
The missing work is cold-start access validity and compatibility evidence,
not absence of a model-decoding algorithm. Parsing S/SDIO or E/UART does not
enable those combinations. Raw unrelated bits are retained, not interpreted
as an established hardware revision.

The fixed command is ``READM 0x40019000 4 `` (19 bytes, trailing space).
The helper exposes no address argument or register write. It requests up to
512 reply bytes to detect oversized/error replies, requires exact successful
OUT length and exactly four IN bytes, decodes little endian only afterward,
and preserves caller output on failure. All operations share an absolute
deadline and cancellation gate. Zero maps to U/USB; it is not rejected solely
because a failed or unfilled buffer might also contain zero. Transfer status,
actual length and captured bytes are checked independently. Repeated zeros
alone do not prove register-access validity or cold-power stability.

The USB parent validates boot interface protocol and endpoint roles, then
issues one query from a worker. It does not select a firmware profile, issue
WRITEM/FILES, reset the chip or publish a functional child. On timeout/error
it remains bound without retrying; teardown closes admission and drains its
URBs/worker. USB cancellation does not establish device-side reply flushing.
Rebinding or QMP reattachment is not a cold-power test or proof of stale
response removal. Automatic loading remains disabled.

The first allocated-U observation had unknown power history. Subsequent
user-confirmed power cycles on the same allocated U produced successful
19-byte OUT and four-byte ``00000000`` IN, decoded U/USB, before preparation
writes. These are same-sample cold-read observations without independent
power instrumentation. The fully supervised native-HCI trial now verifies
eleven preparation ACKs, seven original-file blocks and QUIT, a controlled
QMP handoff, U/USB runtime identity and versions (firmware 0x010a6e3c,
ROM 0x00010101), startup/disable and native HCI initialization/auto-off.
Its 23 real HCI command/reply pairs, management registration/removal,
complete runtime RX captures and three drains passed. Ordinary automatic
loading/publication remains disabled pending board/address requirements.

Prior failures, including a thirteen-event text-capture loss and the first
native-HCI attempt's management-removal observer bug, remain FAIL. The later
success does not retroactively qualify them. Natural in-VM reenumeration,
E cold starts, other samples, address provisioning, board applicability,
RF/peer business and full physical lifecycle qualification remain open.
See :doc:`ws73-qualification-20261017` for each attempt and its consumed
confirmation. Other-project U/E software-reset reports remain references only.

Validation and resources
------------------------

Preserve codec regressions and add production-path tests for firmware roles,
U/E mixing, malformed/truncated input, state transitions, late replies,
USB short transfers, cancellation and every partial-initialization failure.
Run KASAN/UBSAN and separate suitable KCSAN/lockdep configurations. Fuzz
actual parsers and state transitions; turn counterexamples into regressions.
Bounded C verification and TLC must map to actual production behavior;
LKMM is required only for relevant lockless ordering. Unsupported, timed-out
or incomplete verification stays unproved.

Build BLE-only, Wi-Fi-only and combined legal built-in/module configurations;
check GCC/Clang and cross-compile arm64, 32-bit and big-endian targets.
Cross-compilation is not hardware qualification. Preserve precise source,
dirty patch, tools, configuration, artifacts, firmware, topology and logs.
Missing cases, SKIP, old artifacts or incomplete logs must not produce PASS.

Initial physical regression targets are 20 starts and 100 function cycles
per package, plus at least 24 hours of sustained peer-verified traffic before
the relevant formal series. A cold start requires evidence of actual state
clearing. Natural and injected faults remain separate. PM paths unavailable
in the VM are explicitly untested; do not work around VM-only restrictions.

The 2026-10-18 user allocation assigns U ports 1-1.1.3/1-1.1.4 and
E ports 1-2.1.3/1-2.1.4 exclusively to this thread. Ports 1-1.1.1/1-1.1.2
and 1-2.1.1/1-2.1.2 belong to the other thread and must not be operated.
These are topology ports, not enclosure labels. Both hubs share PCI controller
0000:c5:00.3; never pass through, reset or power-cycle either hub/controller.
Recheck port, hub, exact serial, descriptors and ownership before every
individual-device VM attachment. Stop for conflicts or ambiguous mapping;
never select by scan order, device number or fallback to an old port.

Four new per-device boot HCC observations agree with the assigned U/E groups;
they do not qualify cold-ROM automatic selection, new-device runtime startup,
BLE, mixed operation or recovery. Previous U PASS belongs to its original
serial/epoch at the now-excluded port. E hardware is now allocated, but E
qualification, controlled BLE peer and Wi-Fi AP remain outstanding.
Keep SDK110 code/Wi-Fi/BTC files separated by explicit board profile and hash;
Native-HCI callers now accept an explicit U or E trial profile after the
separate E startup/disable result. E HCI still requires its own physical
qualification; this does not enable ordinary automatic functional binding.
The existing RAM/runtime-observation caller now accepts an explicit U or E
profile with separate three-file VM images and package/DMI/HCC agreement;
both passed empty-VM preflight. One newly allocated E subsequently passed
three-file loading and runtime identity/version observation under controlled
QMP handoff. The separately recorded E BLE startup trial below now covers
enable/disable. Other samples, mixed operation and natural recovery remain
open; the preceding system-QEMU E failure is retained.

Allocated E BLE startup observation (2026-10-22)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Source 95e16d0b2a0470ec468c387df2ea2e3cdd37a23c passed a fresh four-job
hardware build and empty-VM preflight before one physical trial on the
allocated E at 1-2.1.3. The user reported unplug/replug; host logs show the
matching serial disconnect at 23:43:41 +08 and boot enumeration at 23:43:43.
There was no independent power instrumentation or new explicit confirmation
about other power sources, so this record does not qualify cold-power behavior.

The VM used only the three pinned original firmware/e files. Boot READM
returned 0x0000000a before preparation. Eleven preparation acknowledgements,
eight file blocks carrying 192632 payload bytes and QUIT completed. Controlled
QMP boot detach/runtime reattach produced a new owner; runtime HCC reported
package 2 / USB and firmware 0x010a6e3c / ROM 0x00010101. Reference
customization, BLE enable/BOOT and acknowledged disable then completed.

Record load-ble-trial-20261022T154737.285550Z has manifest SHA-256
70c85ccccd51e27b1b4f04487ccea745e2c659d79802b216889c4aa44c0a4e0a.
Its supervisor checked 31 boot OUT transfers and 30 replies, eight runtime
bulk OUT transfers, six complete runtime bulk replies, four notifications and
two class commands. Three callback drains completed, QEMU exited with status
zero and the selected device was released. Both USB objects had all four
reset/host-detach opt-outs verified; the trace contains two opens/two closes
on the selected device's two epochs and no usb_host_reset event.

This is one E startup/disable result, not native HCI, peer/RF, address-source,
board-calibration, natural transition/recovery or mixed-device qualification.
The final 352 customization padding bytes were not captured by the 512-byte
TX capture; all 140 input bytes and complete RX records were captured. The
other thread's live VM and devices were not operated. Its shared VM-budget
marker coexisted with this trial's marker; this trial held the selected-port
lease and the existing global build lock. Earlier failures remain retained.

Use private VM/QMP/log/build directories and coordinated whole-machine
budgets: at most 12 build jobs when exclusive, 8 with hardware regressions.
The existing local build.lock now aliases the shared .dev/main-build.lock
inode; its contention check passed. Existing four-job builders remain
serialized, and pre-existing uncoordinated work must still be inspected.

Delivery
--------

Keep signed, DCO-tagged development commits; do not rewrite published
history for review cleanup. Prepare review branches against the appropriate
current subsystem trees. Separate moves from behavioral changes and retain
buildable intermediate commits. Shared core and consumers must not be
submitted as an unused framework. Update MAINTAINERS and reproduction docs.

Firmware delivery preparation runs in parallel: original files, roles,
version/package-specific names, WHENCE, actual license and vendor provenance.
The user's local SDK authorization remains in effect. No binary or private
board material is published without the applicable delivery evidence.
Prepare exact cover letters, patches and recipients before requesting the
user's authorization for external mail or submission to another project.
Partial progress never closes an entire issue or claims mainline acceptance.

Current implementation increment
--------------------------------

Shared HCC helpers are now in drivers/misc/ws73, linked without Bluetooth or
mac80211. Their original 13 regression cases passed unchanged at 92f2e06c7800.
Firmware requests use ``hisilicon/ws73/1.10.110/{us,e}/`` and the three original
basenames. All files are requested and verified before publishing a bundle;
on failure all acquired firmware references are released. The profile table
is selected only from the caller's verified live HCC identity. The helper
itself does not turn an arbitrary struct or eFuse byte into trusted identity.

The download executor consumes already validated payload views after the
caller completes board preparation. It uses 32 KiB chunks, SDK start/middle/
end/single flags, READY, a minimum 5 ms wait, and FILES OK. I/O shares an
absolute deadline, with at most 2000 ms for each operation. Short I/O,
malformed replies, cancellation or errors stop immediately; ambiguous writes
are not replayed. In addition to bare tokens with optional NUL padding,
WRITEM accepts a colon and the exact issued command, and READY accepts the
observed ``:FILE01:`` tag and exact current FILES command. Complete allocated-U
captures establish those echoes and ``FILES OK:`` followed by the exact current
FILES command. All forms retain strict length and NUL-padding checks, including
stale-block rejection. Acknowledged preparation is not evidence
of firmware execution or final RF calibration.

One further SDK correction: MSG_FROM_DEV_QUIT_OK is the EMPTY string. USB
firmware_send_quit_func waits for hcc_usb_reload instead of a text ACK.
Therefore the executor returns WAIT_RUNTIME after sending QUIT; its success
is not runtime readiness or identity verification. No USB download caller is enabled
before the boot identity/board prerequisites are established.
The SDK exec_quit_type_cmd and libws73 send_cmd both append a space: the
wire command is five bytes, ``QUIT ``. The initial synthetic transcript
incorrectly used four bytes. It has been corrected together with production
code; earlier test passes did not establish this wire-format property.


Allocated E native HCI initial experiment (2026-10-24)
------------------------------------------------------

The second allocated E sample, 1-2.1.4, now has a separate physical native
HCI result on source e5a4274d2f20. An explicitly non-cold initial-boot entry
accepted only its pinned read-only observation and unchanged/unowned boot
interface, rejecting prior mutating/unknown attempts. The full existing
SDK110 E three-file load and controlled handoff were retained: fresh boot
HCC identity, all-input validation, runtime E/USB/version verification,
reference customization, BLE startup/disable, then native HCI.

Twenty-four real HCI command/reply pairs, management Read Info, acknowledged
native auto-off, three drains and VM exit zero passed. Reset/detach opt-outs
were false for both individual device epochs, with no QEMU reset recorded.
The entry is consumed by this attempt and cannot be reused without a new
qualified state. This is not a cold-start, board/address, peer/RF, mixed-device
or natural-recovery result; both-model formal qualification remains open.

See the separately dated physical result in ws73-qualification-20261017.rst
and issues 7/12. Raw firmware-bearing captures remain private. The current
RFC review packet and external-mail authorization request are separate from
this later physical experiment.


Allocated U/E native HCI sample coverage (2026-10-24)
-----------------------------------------------------

Newly allocated U 1-1.1.3 independently completed the same full experimental
SDK110 US three-file path and runtime U/USB verification before native HCI.
It has 23 actual command/reply pairs; E 1-2.1.4 has its own separate 24-pair
result. Both use image source e5a4274d2f20, explicit per-board original files,
reference profiles, reset-free individual passthrough and acknowledged
native close/removal with three drains. Both VMs exited zero.

The U admission has its own exact four-byte zero READM observation and
reviewed read-only census hashes; an old other-device power confirmation
is not a power report for this U. Forty-six shared admission checks and six
CLI rejection cases passed. Each physical attempt consumes only its own
initial route, with no automatic retry or port/profile fallback.

These are independent sequential NON-COLD sample initializations. Addresses
were observed to differ without establishing factory origin or uniqueness.
RF/peer/GATT, physical simultaneous isolation, cold selection, board/address,
natural recovery and formal U/E qualification remain separate open work.
See the source-bound qualification records and issues 7/12.


Runtime observation after native close (2026-10-24)
---------------------------------------------------

Allocated U 1-1.1.3 and E 1-2.1.4 independently answered fresh HCC identity
and version queries in new observation-only VMs without replugging. Each
had four successful finite bulk OUTs, two complete replies, clean drain and
VM exit zero. Source is e5a4274d2f20; whole query TX and reply RX capture
checks are attributed separately from the original trial manifests.

Neither device returned BSP_READY in the 6.5-second post-query window.
This does not establish functional warm reentry: the startup BSP gate stays
closed, old state is not carried over, and no BLE function was enabled.
The two initial supervisor false failures and later direct passes are all
retained; 41 negative verifier controls passed. See the source-bound runtime
observations in ws73-qualification-20261017.rst and issues 10/12. Cold, natural
recovery, board/address and peer qualification remain open.


Fixed register read before reset investigation (2026-10-24)
-----------------------------------------------------------

The existing tagged-VM trial has an explicit inspect_reset=1 mode, mutually
exclusive with native HCI startup. It independently verifies runtime HCC
identity/version, then performs only the fixed register-endpoint read of
0x4001f118. There is no generic address or register-data-write interface,
no fallback value, no reset and no firmware/profile request in this mode.

Source 4e0a63b50155 passed the hardware build. Both allocated U 1-1.1.3 and
E 1-2.1.4 returned actual successful four-byte zero values, with complete
TX/RX capture checks, two owner drains, VM exit zero and zero QEMU resets.
Twenty-four pure transaction checks passed in each U/E empty VM; 59 negative
capture checks passed. The first U supervisor false failure and its separate
replay assessment remain recorded alongside the later direct U/E passes.

The next recovery step still needs an explicit bounded per-device reset
operation, its quiescence/ambiguous-write/disconnect handling, target-isolated
boot recognition and a fresh full startup. This read is not a reset or proof
of cleared firmware state, cold startup, natural recovery or RF operation.
See the independently source-bound qualification and issues 10/12.


Controlled software reset investigation (2026-10-24)
----------------------------------------------------

The existing VM trial now has a one-attempt, controller-closed device-reset
mode after fresh HCC identity and a valid fixed-register read. It preserves
the original value while setting bit 4, rejects already-set bits, records
ambiguous write outcomes and leaves the old owner terminal. There is no
retry, fallback, generic register write or hub/controller operation.

U had one actual update followed by new boot descriptors and a separate
verified boot HCC exchange. Its two original FAILs remain: missing explicit
driver probing in the first VM and duplicate version-log counting in the
second. Separate complete-wire replay establishes only the reported subset.
E directly passed the corrected controlled attempt/new-boot supervisor with
fresh E/USB and ROM replies, three drains and exit zero. No second U reset
or automatic retry was performed; both initial reset routes are consumed.

The build, U/E empty-VM 24+32 pure checks, 20 admission cases and 80 capture
negative controls passed. The six other cached device entries and both hubs
were unchanged. These are controlled software experiments, not cold boot,
natural recovery or continuing peer-business isolation. The next function
step must use freshly verified post-reset boot state and the complete
selected-board three-original-file/runtime/startup path; it cannot reuse an
old power confirmation, old owner or a fabricated BSP_READY. See the exact
U/E artifacts and failures in ws73-qualification-20261017.rst and issues 10/12.


Post-software-reset full initialization (2026-10-24)
----------------------------------------------------

A separate --post-reset gate in the existing full-load supervisor now binds
exact reset/boot captures to the current allocated boot device, rechecks the
actual parsers and immutable inputs, and rejects unknown later use or replay.
It does not reuse initial-ROM/power confirmations or promote U's earlier
FAILs. Twenty-two new admission checks, seven CLI rejections and U/E empty
VM preflights passed. No kernel behavior changed in this increment.

Both U 1-1.1.3 and E 1-2.1.4 directly passed the complete selected-board SDK110
three-original-file load, new runtime package/USB/version verification,
genuine startup/disable and native HCI on source 7aa4105b4de9. U has 23 real
command/reply pairs and E 24; both have wire-confirmed native close, three
drains and exit zero. No reset or physical replug occurred during these loads.
Addresses matched each sample's earlier observation without proving origin.

Only the intended boot/runtime targets changed, with no QEMU reset and no
hub/controller or other-thread operations. Their load gates are consumed.
Cold selection, natural/automatic recovery, RF/peer/board/address policy,
repetition/long-duration and formal U/E qualification remain open. The next
native-function-cycle work must preserve a qualified live owner or establish
a new accurately closed/reset/boot state; no old BSP or power state can be
fabricated. Exact artifacts, capture limits and retained FAILs are in
ws73-qualification-20261017.rst and issues 7/10/12.
