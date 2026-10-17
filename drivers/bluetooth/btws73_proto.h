/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __BTWS73_PROTO_H
#define __BTWS73_PROTO_H

#include <linux/ws73.h>

/* Validate exactly one H4 packet, with a direction-specific type allowlist. */
int btws73_hci_validate(const u8 *data, size_t len, bool tx);

/*
 * Encode one BLE HCC record, including zeroed 8-byte USB alignment padding.
 * The caller supplies nonoverlapping input/output. Returns the encoded size
 * or a negative errno without modifying output. This is not a USB envelope.
 */
int btws73_hcc_encode(u8 *out, size_t capacity, const u8 *h4, size_t len);

struct btws73_h4 {
	u16 len;
	bool failed;
	u8 data[BTWS73_H4_MAX_SIZE];
};

/* Reset only after the old RX producer is stopped and drained. */
void btws73_h4_reset(struct btws73_h4 *stream);

/*
 * Serialized RX stream, initially zeroed. Consume at most one packet from
 * validated HCC fragments. Input must not overlap stream storage. Return 1
 * with a complete H4 packet, 0 for a partial packet, or a negative errno.
 * *consumed counts input bytes accepted, including a rejected header. On
 * malformed wire input the stream stays failed until explicitly reset.
 * Only success changes *packet and *packet_len; the view borrows stream storage
 * until the next feed/reset. Consumers must copy before feeding again.
 * This function neither allocates nor calls the Bluetooth stack.
 */
int btws73_h4_feed(struct btws73_h4 *stream, const u8 *data, size_t len,
		   size_t *consumed, const u8 **packet, size_t *packet_len);

#endif
