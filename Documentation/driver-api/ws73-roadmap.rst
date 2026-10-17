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

Only physical device 1-1.1.1 under hub 1-1.1 is currently assigned. Recheck
topology, serial, IDs and ownership before every transfer of ownership.
Never claim/reset the shared PCI controller, old hub or old four devices.
An independently allocated E, controlled BLE peer and Wi-Fi AP are external
test dependencies. Do not borrow the main project's hardware or results.
Use private VM/QMP/log/build directories and coordinated whole-machine
budgets: at most 12 build jobs when exclusive, 8 with hardware regressions.

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
are not replayed. Replies currently accept exact tokens plus optional NUL
padding, pending real 110 capture confirmation.

One further SDK correction: MSG_FROM_DEV_QUIT_OK is the EMPTY string. USB
firmware_send_quit_func waits for hcc_usb_reload instead of a text ACK.
Therefore the executor returns WAIT_RUNTIME after sending QUIT; its success
is not runtime readiness or identity verification. No USB caller is enabled
before the boot identity/board prerequisites are established.
The SDK exec_quit_type_cmd and libws73 send_cmd both append a space: the
wire command is five bytes, ``QUIT ``. The initial synthetic transcript
incorrectly used four bytes. It has been corrected together with production
code; earlier test passes did not establish this wire-format property.
