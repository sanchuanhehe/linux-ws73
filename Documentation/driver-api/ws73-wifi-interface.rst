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
