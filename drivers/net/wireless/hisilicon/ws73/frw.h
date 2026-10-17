/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_WIFI_FRW_H
#define _WS73_WIFI_FRW_H

#include <linux/types.h>

/* Local initial control bound, not a device credit or firmware size claim. */
#define WS73_FRW_DATA_MAX 1024
#define WS73_FRW_HEAD_LEN 36

struct ws73_frw_key {
	u16 id;
	u8 vap;
	u32 sequence;
};

struct ws73_frw_reply {
	s32 device_status;
	const u8 *data;
	size_t length;
};

/* FRW bytes only: caller supplies HCC/USB framing and zeroed DMA padding.
 * Inputs and output must be retained, non-overlapping objects. Failure leaves
 * output unchanged. No command whitelist or firmware compatibility is implied.
 */
int ws73_frw_request_encode(u8 *out, size_t capacity,
			    const struct ws73_frw_key *key, const u8 *data,
			    size_t length, size_t reply_capacity);
/* Exact unpadded FRW record. Success borrows input storage; no device address
 * becomes a host pointer. Parsing success is separate from device_status.
 * Caller must enforce an active transaction, unique sequence and device epoch;
 * tuple matching alone cannot prove freshness or a firmware reset.
 */
int ws73_frw_reply_decode(const u8 *data, size_t length,
			  const struct ws73_frw_key *key, size_t reply_capacity,
			  struct ws73_frw_reply *reply);

#endif
