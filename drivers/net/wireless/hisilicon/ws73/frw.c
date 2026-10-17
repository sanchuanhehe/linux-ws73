// SPDX-License-Identifier: GPL-2.0-only
/* Independent byte codec for the SDK110 candidate FRW control layout. */
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include "frw.h"

#define WS73_FRW_EXT_LEN 8
#define WS73_FRW_COMM_LEN 28
#define WS73_FRW_RSP 1

int ws73_frw_request_encode(u8 *out, size_t capacity,
			    const struct ws73_frw_key *key, const u8 *data,
			    size_t length, size_t reply_capacity)
{
	size_t size;
	u8 *comm;

	if (!out || !key || (!data && length) || key->vap > 15)
		return -EINVAL;
	if (length > WS73_FRW_DATA_MAX || reply_capacity > WS73_FRW_DATA_MAX)
		return -EMSGSIZE;
	size = WS73_FRW_HEAD_LEN + length;
	if (capacity < size)
		return -ENOSPC;

	memset(out, 0, size);
	put_unaligned_le16(key->id, out);
	out[2] = key->vap;
	put_unaligned_le16(WS73_FRW_COMM_LEN + length, out + 3);
	comm = out + WS73_FRW_EXT_LEN;
	put_unaligned_le16(key->id, comm);
	comm[2] = key->vap;
	comm[3] = 1; /* Synchronous CFG; type is zero. */
	put_unaligned_le32(key->sequence, comm + 8);
	put_unaligned_le16(length, comm + 20);
	put_unaligned_le16(reply_capacity, comm + 22);
	comm[26] = 1; /* Nested synchronous flag; type/reserved are zero. */
	if (length)
		memcpy(out + WS73_FRW_HEAD_LEN, data, length);
	return size;
}

int ws73_frw_reply_decode(const u8 *data, size_t length,
			  const struct ws73_frw_key *key, size_t reply_capacity,
			  struct ws73_frw_reply *reply)
{
	struct ws73_frw_reply parsed;
	const u8 *comm;
	u16 payload;

	if (!data || !key || !reply || key->vap > 15)
		return -EINVAL;
	if (reply_capacity > WS73_FRW_DATA_MAX || length < WS73_FRW_HEAD_LEN ||
	    length > WS73_FRW_HEAD_LEN + WS73_FRW_DATA_MAX)
		return -EMSGSIZE;
	comm = data + WS73_FRW_EXT_LEN;
	if (data[2] >> 4 != WS73_FRW_RSP ||
	    ((comm[3] >> 1) & 7) != WS73_FRW_RSP)
		return -EPROTO;
	/* Only the plain synchronous configuration envelope is supported. */
	if (data[5] || get_unaligned_le16(data + 6) || comm[3] != 3)
		return -EPROTO;
	if (get_unaligned_le16(data) != get_unaligned_le16(comm) ||
	    (data[2] & 15) != comm[2])
		return -EPROTO;
	if (get_unaligned_le16(comm) != key->id || comm[2] != key->vap ||
	    get_unaligned_le32(comm + 8) != key->sequence)
		return -ENOENT;
	payload = get_unaligned_le16(comm + 24);
	if (payload > reply_capacity || payload > get_unaligned_le16(comm + 22))
		return -EMSGSIZE;
	if (get_unaligned_le16(data + 3) != WS73_FRW_COMM_LEN + payload ||
	    length != WS73_FRW_HEAD_LEN + payload)
		return -EMSGSIZE;
	/* Remote pointer slots, request data_len and nested flags are not used. */
	parsed.device_status = (s32)get_unaligned_le32(comm + 4);
	parsed.data = data + WS73_FRW_HEAD_LEN;
	parsed.length = payload;
	*reply = parsed;
	return 0;
}
