/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __BTWS73_PROTO_H
#define __BTWS73_PROTO_H

#include <linux/types.h>

#define BTWS73_RX_HEADER_SIZE	92
#define BTWS73_RX_MAX_RECORDS	24
#define BTWS73_RX_MAX_RECORD_SIZE	4096
#define BTWS73_HCC_HEADER_SIZE	4
#define BTWS73_H4_MAX_SIZE	1500
#define BTWS73_RX_MAX_SIZE	(BTWS73_RX_HEADER_SIZE + \
				 BTWS73_RX_MAX_RECORDS * BTWS73_RX_MAX_RECORD_SIZE)

struct btws73_fragment {
	const u8 *data;
	u16 len;
};

struct btws73_rx {
	u8 count;
	struct btws73_fragment fragment[BTWS73_RX_MAX_RECORDS];
};

/*
 * Decode one complete USB aggregate on the BLE queue. The returned views
 * borrow the input: it must remain immutable and alive until consumed.
 * Fragments are H4 bytes, not necessarily complete HCI packets. No callbacks,
 * allocations or state changes occur; *rx is unchanged on any error.
 */
int btws73_hcc_decode(const u8 *data, size_t len, struct btws73_rx *rx);

/* Validate exactly one H4 packet, with a direction-specific type allowlist. */
int btws73_hci_validate(const u8 *data, size_t len, bool tx);

/*
 * Encode one BLE HCC record, including zeroed 8-byte USB alignment padding.
 * The caller supplies nonoverlapping input/output. Returns the encoded size
 * or a negative errno without modifying output. This is not a USB envelope.
 */
int btws73_hcc_encode(u8 *out, size_t capacity, const u8 *h4, size_t len);

#endif
