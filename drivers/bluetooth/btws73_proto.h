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

#endif
