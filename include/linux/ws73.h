/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_WS73_H
#define _LINUX_WS73_H

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

/* SDK wire values: package U/S/E = 0/1/2; bus USB/SDIO/UART = 0/1/2. */
struct btws73_chip_identity {
	u8 package;
	u8 bus;
};

struct ws73_version {
	u32 firmware;
	u32 rom;
};

/* These encode/decode HCC records, not USB transactions or live identity. */
int btws73_chip_query_encode(u8 *out, size_t capacity);
int btws73_chip_reply_decode(const u8 *data, size_t len,
			     struct btws73_chip_identity *identity);
int ws73_version_query_encode(u8 *out, size_t capacity);
int ws73_version_reply_decode(const u8 *data, size_t len, struct ws73_version *version);

/*
 * Decode one complete USB aggregate on the BLE queue. The returned views
 * borrow the input: it must remain immutable and alive until consumed.
 * Fragments are H4 bytes, not necessarily complete HCI packets. No callbacks,
 * allocations or state changes occur; *rx is unchanged on any error.
 */
int btws73_hcc_decode(const u8 *data, size_t len, struct btws73_rx *rx);

#endif
