.. SPDX-License-Identifier: GPL-2.0-only

WS73 SDK110 Wi-Fi interface findings
====================================

Scope and sources
-----------------

This supplements the existing roadmap and Wi-Fi TX-status/RX-security audit.
It does not change the mac80211 implementation route or establish working STA
support. The source below is the local SDK at
``d5e68a58fe38c2b04a16769c69de1e8a80e86d74``. Twenty-three inspected files
were byte-compared to that commit. The private record
``wifi-framing-control-audit-20261017/manifest.json`` has SHA-256
``4ae3b2bf157ec66955cab896447b1bf19aa33ad671b5a82e22ffa296009159b8``.
Paths below are relative to ``driver/wifi`` unless otherwise stated.

Control and frame paths differ
-----------------------------

``frw/frw_hcc_adapt.c:frw_tx_wifi_cfg_by_hcc`` selects HCC customization
service 4, subtype CUSTOM_CFGID_MSG (4), and control queue 0. This is distinct
from BLE customization service 5. ``frw_send_msg_adapt_by_hcc`` prepends an
eight-byte FRW extension and a fixed-width communication envelope to command
data; its FRW data_len includes that envelope. It rounds the whole allocation
to the DMA alignment before submitting it. Allocation padding is not extra
command data. Reply matching needs message ID, sequence, VAP and device epoch;
the existing Host ``frw/frw_thread.c:frw_find_msg_node`` matches ID/sequence.
Unlike the platform BLE queries, this envelope has a sequence field.

``frw_tx_wifi_netbuf_by_hcc`` selects service 6, subtype 0. Default management
and data queues are 1 and 2; optional queue classification can change the
data queue. ``frw/frw_hmac_adapt.c:frw_hmac_send_data`` supplies FRW extension,
32-byte copied control block, 36-byte reserved MAC-header region and body.
Here FRW data_len describes the body after removing the actual MAC header,
not the control block/header reservations or entire record. PK-mode and
AMSDU variants require separate treatment. A generic parser cannot assign
configuration-length semantics to frame records.

The ordinary non-AMSDU path in ``forward/hmac_tx_encap.c:hmac_tx_encap_etc``
starts with Ethernet, builds SNAP/LLC and then constructs an 802.11 header
on the Host. ``forward/hmac_tx_mpdu_adapt.c:hmac_tx_send_netbuf`` ultimately
hands it to FRW. This supports the raw 802.11 implementation direction for
that path; it does not prove that every firmware mode takes the same layout.
A mac80211 skb still needs a validated device descriptor and packing step.
Device-side producer/consumer behavior, descriptor fields and per-MPDU TX
results remain dependencies; USB completion cannot provide wireless ACK.

Candidate communication layout
------------------------------

The independent object probe in ``tools/testing/ws73/wifi/check-abi.py``
checks the pinned declarations on five Clang targets. All little-endian
targets agree on the following byte offsets, including RISC-V32:

.. list-table:: FRW extension (8 bytes)
   :header-rows: 1

   * - Offset
     - Field
   * - 0..1
     - Message ID, little-endian u16
   * - 2
     - VAP low nibble, message type high nibble
   * - 3..4
     - data_len, little-endian u16; meaning depends on message type
   * - 5
     - pay_type low nibble, sub_type high nibble
   * - 6..7
     - Reserved

The fixed communication envelope is 28 bytes: ID at 0, VAP at 2,
sync/type/reserved bits at 3, signed callback result at 4, sequence at 8,
32-bit data/rsp slots at 12/16, data_len/rsp_buf_len/rsp_len at 20/22/24,
and nested sync/type/reserved bits at 26. The Host request initializer zeros
the data/rsp slots; a device value in either slot must never become a Host
pointer. Reply bounds must be checked against the actual received record
and the locally requested capacity, not by trusting the remote capacity or
silently clipping an oversized reply as the SDK consumer does.

The pointer-bearing native frw_msg_adapt is 28 bytes on 32-bit and 40 on
64-bit, whereas frw_msg_adapt_comm stays 28. PowerPC64 big-endian also changes
integer byte order and bitfield placement. Thus neither native structures
nor compiler bitfields define a portable Linux wire codec. The probe proves
only recorded compiler layouts; the original firmware compiler/options and
actual response export still require evidence. Its sentinel values are not
valid commands, and no compiled object is executed or sent to hardware.

Scan, channel and aggregation ownership
---------------------------------------

``hmac/hmac_scan.c:hmac_scan_proc_scan_send_event`` directly invokes the
Host scan handler. The inspected scan path constructs probe requests,
switches channels and reports completion from the Host. It does not provide
an established firmware hw_scan request/report ABI. Once channel control and
management TX/RX are qualified, use mac80211's software scan path; do not
import the SDK scan/SME engine. The stack's software/hardware scan contracts
are documented in ``include/net/mac80211.h`` and the
`mac80211 API documentation <https://docs.kernel.org/driver-api/80211/mac80211.html>`_.

``hmac/hmac_chan_mgmt.c:hmac_mgmt_switch_channel`` sends synchronous
DEVICE_SWITCH_CHANNEL with channel number, band, bandwidth, index and
clear_fifo. It logs a failed request but then updates Host channel state and
returns success. This is a source behavior to avoid: a new driver must
propagate failure and retain a truthful old/unknown channel state. A received
callback result, not Host state assignment, must establish a successful
configuration; supported channel/bandwidth and regulatory limits remain gated.

``hmac/hmac_blockack.c`` owns reorder queues/timeouts and contains explicit
device DELBA/AMPDU synchronization. ``hmac_ba_tx_ampdu_switch`` checks the
reply before updating its hardware-aggregation state. These paths do not
justify advertising firmware reorder or aggregation offload. Keep stack BA
and reorder ownership. Establish the unaggregated firmware policy before
first STA operation; add aggregation only with its separate device contract.

Power and regulatory application
--------------------------------

``hmac/hmac_config.c:hmac_config_set_txpower`` and
``hmac/hmac_power.c:hmac_pow_set_vap_tx_power`` compute limits and refresh
rate power tables. ``hal/hh503/hal_power.c:hal_pow_sync_tpc_code_to_dmac``
delegates to ``fe/power_ctrl/fe_tpc_rate_pow.c``, which flattens a u16 table
and sends asynchronous DEVICE_SYNC_TPC. Local enqueue success does not prove
device application or a regulatory power limit. Table generation, board
calibration, units, firmware application/error behavior and readback need
mapping before advertising RF support. Do not use private SDK regulatory
tables or a logged Host success as a substitute for cfg80211 restrictions.

``hmac/hmac_sta_pm.c`` also has separate SET_PS_MODE and SET_STA_PM_ON
requests. Their existence does not qualify device sleep/wakeup or USB PM.
The previous PM/autosuspend gates remain unchanged.

Remaining first-STA dependencies
--------------------------------

The findings narrow concrete implementation work but do not close #3/#8:
confirm the original firmware's descriptor/FRW export; identify ordinary-data
ACK/retry/rate correlation; resolve RX FCS, IV/MIC and replay ownership;
establish acknowledged channel/power application and minimal VAP/user setup;
then implement and qualify mac80211 STA against a controlled AP and data peer
on both U and E. No Wi-Fi adapter, channel or security capability is registered
by this audit, and no substitute Host SME or fabricated TX status is introduced.


RX and TX-status declaration layout probe
----------------------------------------

The probe now also compiles the actual pinned scalar declarations from
``device/source/inc/romable/device_common/hal_common_ops_device_rom.h``,
``device/source/inc/romable/device_common/dmac_ext_if_device_rom.h`` and
``device/source/inc/romable/dmac_ext_if_type_rom.h``. It preserves each HAL
pack(1) scope and leaves the two outer DMAC structures unpacked, as declared.
Explicit sentinel objects and static offset/size assertions pass on x86-64,
i386, arm64, RISC-V32 and PowerPC64 big endian. This verifies candidate compiler
layouts; it does not establish the compiler/options or export of SDK110 ROM.

.. list-table:: Candidate little-endian receive control (32 bytes)
   :header-rows: 1

   * - Byte range
     - Observed declaration layout
   * - 0..3
     - VAP/buffer/fragment fields, user, two-bit tid field, MAC-header length
   * - 4..7
     - LE16 frame length, VAP/buffer count, channel number at byte 7
   * - 8..15
     - FTM/stream/EVM fields, 24-bit latency timestamp, latency index, DA user
   * - 16..19
     - LE32 device virtual MAC-header address; never a Host pointer
   * - 20..23
     - Cipher low nibble/status high nibble, GI/STBC/AMPDU/bandwidth fields
   * - 24..31
     - Signed RSSI, protocol/rate bits, antenna SNR/RSSI, signed phase increment

The declared RX tid field is only two bits; it cannot be treated as a complete
802.11 QoS TID without establishing its meaning. The latency fields do not
provide a declared 48-bit CCMP PN. A valid header reservation, status byte or
RSSI does not establish FCS/IV/MIC retention, authenticated decryption or replay
validation. These decisions still need device-producer and packet evidence.

The TX-status declaration is eight bytes: LE32 descriptor status, one-byte
frame type, one-byte frame marker and LE16 user index. The probe confirms no
retry count or rate field in THIS declaration, not absence of every possible
firmware completion facility. ``forward/hmac_tx_data.c:hmac_tx_eapol_key_proc``
sets need_rsp specifically for PTK EAPOL message 4/4; ordinary data with an
arbitrary need_rsp/marker combination has no demonstrated completion contract.
Big-endian sentinel objects differ in both integer order and bitfield placement.
The new driver must decode explicit bytes, never cast to these C declarations.


Key installation is a multi-step device transaction
--------------------------------------------------

The following additional Host call chain was byte-compared to the same SDK
pin. This is implementation evidence, not a firmware vulnerability claim or
a reason to change the approved mac80211 architecture.

* ``hmac/hmac_11i.c:hmac_11i_update_key_to_ce`` calls the HAL key functions
  and returns success even though those functions have void return types.
  ``inc/hal/hal_ext_if.h`` delegates through the hh503 hook mapping in
  ``device/source/inc/romable/hal_ops_common_rom.h``. The SDK Makefile includes
  both ``hal/hh503/hal_ce.c`` and ``hal/hal_reg_opt.c`` in the Host objects.
* ``hal/hh503/hal_ce.c:hh503_ce_add_key`` clears TX/RX PN when update_key is
  false, enables the cipher engine, programs the key and checks key material.
  Internal failures are logged and return through the void interface.
  ``hh503_clear_rx_tx_pn`` distinguishes PTK QoS, non-QoS and management
  entries as well as group entries. A wrapper success therefore does not prove
  either installation or preservation of the required PN state.
* ``hal/hal_reg_opt.c:hal_reg_write`` routes the 0x40010000 address prefix
  through hcc_write_reg outside the BSLE-gateway build. Other addresses use
  synchronous FRW REG_WRITE, but the wrapper discards its result and returns
  success. REG_READ failure is represented as an all-ones data sentinel.
  That API cannot distinguish a valid all-ones register from a failed read.
* The 16-bit write wrapper may continue to the FRW path after a failed direct
  write. Do not import this implicit replay into a new driver: failure can
  leave device state uncertain, and a second transport attempt is not proof
  that the first write did not take effect. No register is accessed by this
  audit, and no generic register-access userspace ABI is proposed.
* ``device/source/inc/romable/dmac_common_inc_rom.h`` defines reg_info as
  three u32 fields (address, width, value); width enum values are 0 for 16 bits
  and 1 for 32 bits. This identifies a request body, not a qualified whitelist,
  device callback, atomic register transaction or actual register response.
* ``hh503_is_ce_lut_done`` increments a counter by 10 after each register
  poll and requested 10-us delay until the counter reaches 2000. Register
  transactions themselves take time, so this loop is not an absolute two-ms
  deadline. New code needs one measured deadline that includes every request.
* Non-PTK updates may disable replay detection and arm the global
  g_update_gtk_timer in ``hmac_11i.c``. New per-device/per-key ownership must
  account for other active users and for failure/removal during an update;
  copying this global timer does not meet isolation or security contracts.
* ``hmac_en_mic_etc``'s CCMP branch only reports an eight-byte IV length;
  ``hmac_de_mic_etc`` performs a software MIC check only for TKIP. The TX
  descriptor cipher comes from Host user state, with a plaintext EAPOL
  exception in ``forward/hmac_tx_mpdu_adapt.c:hmac_tx_set_cipher_param``.
  This does not prove a raw protected-frame path suitable for software CCMP.

Consequences for the first STA implementation
-------------------------------------------

Implement key installation/removal only after mapping acknowledged device
operations and their scope: serialize the full key/PN transaction, propagate
request and device errors independently of read data, stop on uncertain
state, and enable protected traffic only after confirmed completion. Establish
reinstall/rekey behavior and the relation between stack PN and device PN;
never reset PN merely because an identical key is supplied again. Keep RX
DECRYPTED/IV_STRIPPED/PN_VALIDATED capability decisions tied to actual verified
firmware behavior, not these Host wrappers or descriptor field names.

The concrete remaining evidence is ordinary-data completion correlation and
ACK/retry/rate reporting; original-firmware RX FCS/IV/MIC/PN export; successful
and failed key/PN operations including reinstallation; and either qualified
hardware crypto or a proven unchanged protected-frame path for software crypto.
These are prerequisites for the planned WPA2-CCMP STA, not optional tests to
skip after an unencrypted link succeeds. The framing and layout findings allow
explicit codec design, but do not permit registering a functional Wi-Fi adapter
or claiming the device accepts speculative descriptors. No firmware version
switch, private Host SME or fabricated ACK is introduced.
