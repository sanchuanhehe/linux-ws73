.. SPDX-License-Identifier: GPL-2.0-only

WS73 SDK110 interface evidence requests
=======================================

This is a prepared technical question set for the existing #1--#15 roadmap,
not a new project plan or a sent vendor request. No vendor endorsement or
upstream acceptance is implied. It covers SDK 1.10.110 and both U/USB and
E/USB. Original code, Wi-Fi calibration and BTC calibration files remain
separate. Answers should identify firmware hashes, ROM/revision and board
scope; an answer for one model must not silently authorize the other.

Boot identity and board preparation (#2, #3, #5, #13)
----------------------------------------------------

* Confirm the validity conditions for boot HCC ``READM 0x40019000 4 ``:
  cold-reset accessibility, error/short-transfer behavior, and whether a
  successful four-byte zero reply is a valid U observation for each ROM.
  Document the strap-to-package/bus mapping and its revision boundaries.
  Existing same-sample U cold reads and cross-project soft-reset observations
  do not establish every U/E board or ROM.
* Supply the generated CLDO register-offset definitions referenced by the
  Host RAM2/3/4 calibration preparation. The current numeric map is traced
  to libws73 reference code; the required register meanings, reserved-bit
  constraints and board applicability need a device specification.
* Identify how a Linux driver obtains the board profile, device address and
  XO trim without installing development INI defaults. Distinguish public
  calibration tables from per-device identity/calibration values. State which
  data may be absent and the required failure behavior.
* Explain the precedence of the host value at 0x40019408 and the subsequent
  firmware calibration helper. Identical U/E code initializes bytes to 8/60,
  substitutes a nonzero host word's low two bytes, then passes both byte
  addresses through a helper before using them. Thus an acknowledged write
  does not establish the final applied trim; an all-zero host word also has
  different semantics from two explicitly applied zero trims.
* Provide the supported temperature-compensation policy and how successful
  application is acknowledged. Identify a board/revision-approved reference
  profile for the allocated U and a separately allocated E.

Useful closure evidence: a revisioned register/board specification or matching
source definitions, plus VM-captured cold READM, exact preparation ACKs and
runtime identity/version for each supported model. A broad success-prefix
comparison or a marketing product name does not establish these contracts.

Device address read contract (#3, #7, #13)
-----------------------------------------

The allocated U now completes native HCI initialization and returns consistent
Read BD_ADDR/management identity, but the source of that address is unknown.
Its trial customization contains zero BLE/SLE address fields, and no vendor
address setter is sent. Literal address searches in the six pinned U/E input
files find no match; this does not prove ROM, eFuse, flash or constructed-default
origin, uniqueness, or factory provisioning.

* Supply ``c_union_define_efuse_ctrl_rb.h`` and
  ``efuse_ctrl_rb_reg_offset.h`` for the supported U/E revisions, or equivalent
  register documentation. SDK110's ``hal_soc_reg.h`` references these files,
  but they are absent from the inspected checkout. In particular provide
  masks, polarity and reset/read side effects for efuse_boot_st,
  efuse_ctrl_busy and efuse_rd_st, including stale read-done behavior.
* Confirm the reference addresses 0x40018010/14/18/1c, READ_EN 0x5a5a,
  byte-index meaning of WR_ADDR, and low-byte validity of RDATA. The SDK
  requires ready/not-busy checks before the read and read-done/not-busy before
  consuming data. libws73 reads status three times but does not test its bits;
  it therefore cannot supply the missing completion semantics.
* Confirm supported access epochs and any clock/reset prerequisites for a
  bounded read of only the three MAC slots: SDK IDs 72--74, bits 800/848/896
  with length 48 each (bytes 100--117). No fuse programming, lock modification,
  generic register API or use of the UART-only READ_EFUSE message is proposed.
* Specify slot validity and precedence, byte order, address type/provisioning,
  and the BLE derivation policy. The Host tries slot 74 down to 72 and adds
  one in the last 24 address bits for BLE, with wrap behavior. Confirm collision
  avoidance and how invalid/missing factory data must fail; neither INI nor
  Ethernet-random fallback establishes a provisioned public Bluetooth address.

Useful closure evidence: revisioned generated definitions or a narrow read
contract, matching full-status captures and independently known provisioned
addresses for assigned U/E samples. A nonzero HCI address alone is insufficient.
The shared owner must serialize the entire address-read operation, bound its
absolute deadline including all register transfers, and leave caller output
unchanged on failure. SDK per-register locking is not whole-operation exclusion.

Wi-Fi control replies (#3, #5, #8)
----------------------------------

The pinned Host routes customization service 4/subtype 4 through queue 0.
Its configuration request is HCC plus eight-byte FRW extension and 28-byte
communication envelope, with the allocation rounded to 32 bytes. The current
independent codec models an unpadded synchronous FRW reply but has no physical
transport consumer. Please supply the actual device reply producer or its
wire contract, including:

* HCC length meaning and padding, FRW data_len, nested lengths, reserved bits
  and sync/type flags for both successful and failed synchronous commands.
* ID/VAP/sequence propagation, callback-status meaning, asynchronous messages,
  maximum reply size and behavior on unsupported commands or oversized data.
* Whether late replies may survive a reset/disable/re-enable, and which
  operation establishes a new command epoch. No transaction tuple is reused
  locally, but that alone cannot prove a clean firmware epoch.
* The minimum acknowledged VAP/user/channel setup and teardown sequence for
  a single unaggregated STA, including all required initial platform state.

Useful closure evidence: raw complete request/reply records for U/E SDK110,
with exact actual USB lengths, nonzero callback failure, unsupported-command
failure and delayed-response behavior. A Host decoder that clips lengths or
logs failures cannot establish the device-side format. A compatible ROM map,
symbol file or narrow producer source can resolve ambiguous disassembly;
no general register-writing or ROM-dumping interface is requested.

802.11 data, security and RF control (#3, #8, #9, #13)
-----------------------------------------------------

* Confirm the raw 802.11 TX descriptor/header/body layout and which fields are
  required for management and ordinary data. Provide per-MPDU completion
  correlation, ACK/failure, retries and final-rate reporting. The inspected
  eight-byte dmac_tx_status declaration and queue-scheduling message do not
  establish a general data completion contract.
* Confirm device-produced RX metadata: actual frame/header lengths, channel,
  RSSI/rate, FCS retention/status, aggregation, cipher/result, IV/MIC retention
  and replay/PN ownership. The candidate 32-byte control block includes a
  device address that will never become a Host pointer. Specify which flags
  may truthfully map to mac80211 receive flags.
* Provide acknowledged key add/replace/remove and PN behavior, including
  reinstallation, GTK/PTK changes, wrong keys and removal during an update.
  If software crypto is supported instead, demonstrate unchanged protected
  frames in both directions and the descriptor policy that disables device
  encryption/decryption. The Host's void HAL key wrappers are insufficient
  evidence of successful installation.
* Specify acknowledged channel/bandwidth and power-limit application, units,
  board calibration and failure state. The Host channel setter's logged
  failure followed by local success, or asynchronous power-table enqueue,
  cannot establish regulatory-safe device configuration.
* Confirm unaggregated behavior and BA/AMPDU/AMSDU ownership so mac80211 can
  retain scan/SME/reorder responsibilities without importing the private HMAC.

Useful closure evidence: producer/consumer layouts tied to SDK110 U/E plus
controlled AP captures, bidirectional payload checks, real TX status and
security failure/rekey tests. These do not change the selected mac80211 route.
Missing metadata must remain a documented dependency; USB completion cannot
stand in for wireless acknowledgement.

How answers become implementation evidence
------------------------------------------

Keep the received material, source/version and permitted use recorded locally.
Translate only established wire contracts into independent bounded codecs and
state transitions; add malformed/late/cancelled cases before transport use.
Then qualify the actual U/E paths with the allocated devices inside VMs.
Compile, synthetic USB, static analysis and physical results remain separate.
The existing requirement for both models before formal support submission and
the approval boundary for external mail/submissions remain unchanged.
