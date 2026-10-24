.. SPDX-License-Identifier: GPL-2.0-only

WS73 USB owner and Bluetooth LE integration
===========================================

Implementation status
---------------------

The development driver separates a shared USB owner in
``drivers/misc/ws73/`` from the native Bluetooth consumer in
``drivers/bluetooth/btws73.c``. One physical USB interface has one owner.
An auxiliary device connects an initialized BLE function to the consumer.
The shared layer has no Bluetooth or mac80211 protocol dependency.

Ordinary physical USB binding currently performs identity observations only.
It does not download firmware, publish a BLE function or create a wireless
interface. Enabling the configuration options below therefore does not make
a connected WS73 a usable Bluetooth or Wi-Fi adapter. Automatic loading and
function publication require verified cold-ROM identification, board inputs,
device address provisioning and startup completion. These gates remain open.
The directory and auxiliary-bus choices are proposed for maintainer review.

The intended USB combinations are WS73U and WS73E. Decoding an S/SDIO or
E/UART identity does not enable those transports. Wi-Fi is a separate
mac80211 implementation track; this integration does not provide STA, AP,
RF coexistence, power management or automatic recovery. SparkLink/SLE has
no adapter in this series.

Configuration and firmware inputs
---------------------------------

``CONFIG_WS73_USB`` selects the shared core and USB I/O helpers.
``CONFIG_BT_WS73`` depends on that owner, Bluetooth and LE support and
selects the H4/HCC codec. The tristate dependencies prevent a built-in
consumer from referring to a modular owner. The core selects auxiliary bus
and firmware/hash helpers without selecting either protocol stack.

The loader helpers describe SDK 1.10.110 original, separate inputs. Under
the firmware search directory, the U profile requests::

  hisilicon/ws73/1.10.110/us/ws73.bin
  hisilicon/ws73/1.10.110/us/wifi_cali.bin
  hisilicon/ws73/1.10.110/us/btc_cali.bin

The E profile requests::

  hisilicon/ws73/1.10.110/e/ws73.bin
  hisilicon/ws73/1.10.110/e/wifi_cali.bin
  hisilicon/ws73/1.10.110/e/btc_cali.bin

Installing these files alone does not activate the observation-only parent.
The helpers require a validated USB chip identity before selecting one
profile. All three files must pass role, exact size, internal digest and
whole-file digest checks before board preparation or download. Only then
is each 64-byte digest header removed from the transmitted payload. There
is no combined-image or alternate-model fallback. The filenames agree
with ``MODULE_FIRMWARE``; firmware bytes are distributed separately from
the driver and require their own deployment and licensing evidence.

The fixed boot command is ``READM 0x40019000 4`` followed by one ASCII space.
It requires an actual four-byte reply. A successfully received zero
can be a U observation; a zero-initialized or short receive buffer is not
an identity. Observed strap decoding is not qualification of automatic
selection across cold U/E hardware. Runtime HCC identity and version must
be checked again in each new runtime instance.

Ownership and lifetime
----------------------

The physical owner must establish compatible identity/firmware, applicable
board and address inputs, platform configuration and real startup replies.
It must disable and drain the initial BLE session before publishing the
auxiliary endpoint. A caller-supplied flag cannot establish those facts.
Boot and runtime USB instances retain independent resources across
reenumeration; old URBs, replies and cached identity do not transfer.

The internal API is declared in ``include/linux/ws73-ble.h``. Start, stop
and send are sleepable, serialized backend operations. Start and stop
require actual device acknowledgements. Send borrows its HCC record until
return; success describes local transport only. Receive/fault callbacks
run serialized under an IRQ-safe lock: they must not sleep or reenter the
shared API. Unbind drains those callbacks. A retained device reference
pins the endpoint allocation through removal, but does not permit access
to a removed backend.

The HCI consumer accepts command/ACL skbs. On successful send it owns the
skb; on error the caller retains it. Packet and byte bounds include the
active TX worker. Receive copies validated event/ACL data into owned skbs
before passing it to Bluetooth; URB memory does not escape its lifetime.
Native Bluetooth owns HCI command/data credits. Neither USB completion nor
shared transport completion substitutes for a controller event or radio ACK.

The first terminal failure closes admission. A BLE-local failure does not
reset another function; the owner classifies whole-chip failures. Teardown
aborts waiters, stops producers, cancels and joins USB I/O and work, detaches
function routing and removes auxiliary consumers before freeing owner state.
Active backend calls and callbacks must be drained. HCI withdrawal cancels
pending synchronous commands before unregistering the controller. Every
nonnegative ``hci_register_dev()`` return is a successful allocated index.
There is no in-place API to clear a terminal error into a new ready epoch.

Reproduction and qualification
------------------------------

Run experimental kernels and modules only inside VMs. The hardware-free
tools use dummy_hcd/FunctionFS, separate synthetic USB IDs and an explicit
controller reply model. They assign no physical USB, disk or network to
the guest. From a clean kernel worktree, the existing tools include::

  python3 tools/testing/ws73/run-kunit.py --combined-hci
  python3 tools/testing/ws73/usb-vm/build.py
  python3 tools/testing/ws73/usb-vm/run.py --phase hci --speed full
  python3 tools/testing/ws73/usb-vm/run.py --phase hci --speed high

See ``tools/testing/ws73/usb-vm/README`` for dependencies, input checks,
other phases and module lifecycle profiles. Reserve the documented four
build jobs and two VM CPUs within the shared machine budget. Build outputs
and records live in the sibling ``.dev/ws73-upstream`` directory; its shared
``build.lock`` serializes builds. Do not execute the compiled PID-1 fixture
on the host. The loader phase additionally needs authorized original
firmware and preserves those inputs only in local test artifacts.

Tests cover production parsers and bounded transactions, malformed records,
late replies, teardown during active operations, native HCI rebinding with
a nonzero index and two independent synthetic HCI endpoints. They do not
establish physical initialization, address provenance, firmware epoch
clearing, real connections or RF behavior. Build, synthetic USB, physical
device and peer results must remain separately tied to their exact source,
configuration, input hashes, raw logs and all failed attempts.

Historical U native HCI command exchanges and an E BLE startup/disable trial
are limited experiments with their original sources and reference profiles.
They do not qualify this review tree or establish E native HCI, U/E peer
traffic, cold automatic loading, natural recovery or formal support. Both
models need the relevant real-device evidence before a formal hardware
support series. RFC preparation, submission, maintainer acceptance and
mainline integration remain distinct milestones.
