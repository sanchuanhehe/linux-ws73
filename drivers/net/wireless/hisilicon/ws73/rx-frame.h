/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_WIFI_RX_FRAME_H
#define _WS73_WIFI_RX_FRAME_H

#include <linux/types.h>

#define WS73_WIFI_RX_CB_SIZE 32
#define WS73_WIFI_RX_HEADER_RESERVE 36
/* Initial local limit for an unaggregated receive allocation, not an MTU. */
#define WS73_WIFI_RX_RECORD_MAX 4096

struct ws73_wifi_rx_frame {
	const u8 *header;
	const u8 *body;
	size_t header_length;
	size_t body_length;
	u8 channel;
	s8 signal;
	u8 cipher;
	u8 status;
};

/* Candidate SDK110 non-WS53, single-buffer, non-AMSDU record after FRW removal:
 * 32-byte control block, 36-byte header reservation, body, optional <=31 DMA
 * padding bytes. Bounds are validated before publishing either borrowed span.
 * The device header address is never followed. Caller retains immutable input;
 * input/output must not overlap. Error leaves output unchanged.
 *
 * This is structural decoding only: caller must validate the HCC/FRW envelope,
 * VAP/epoch, 802.11 header and metadata against a qualified firmware profile.
 * Raw cipher/status are not mac80211 decryption, FCS or replay flags. No cipher
 * overhead is removed, and neither channel nor signal establishes RF support.
 */
int ws73_wifi_rx_frame_decode(const u8 *data, size_t length,
			      struct ws73_wifi_rx_frame *frame);

#endif
