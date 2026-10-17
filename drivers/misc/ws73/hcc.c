// SPDX-License-Identifier: GPL-2.0-only
/* WS73 shared wire validation. See Documentation/driver-api/ws73.rst. */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <linux/ws73.h>

#define BTWS73_USB_RX_AGGREGATE	2
#define BTWS73_BLE_QUEUE		7

int btws73_chip_query_encode(u8 *out, size_t capacity)
{
	if (!out)
		return -EINVAL;
	if (capacity < 32)
		return -ENOSPC;

	/* OAM service 1 / platform subtype 9, queue 9, padded total length. */
	memset(out, 0, 32);
	out[0] = 0x19;
	out[1] = 9;
	put_unaligned_le16(32, out + 2);
	put_unaligned_le16(16, out + 4); /* GET_CHIP_TYPE */
	put_unaligned_le16(1, out + 6);
	/* One zero request byte at offset 8; remaining bytes are padding. */
	return 32;
}
EXPORT_SYMBOL_NS_GPL(btws73_chip_query_encode, "WS73");

int btws73_chip_reply_decode(const u8 *data, size_t len,
			     struct btws73_chip_identity *identity)
{
	struct btws73_chip_identity parsed;
	u16 record_len, message_len;

	if (!data || !identity)
		return -EINVAL;
	if (len < 18 || len > BTWS73_RX_MAX_RECORD_SIZE)
		return -EMSGSIZE;
	if (data[0] != 0x19 || data[1] != 9)
		return -EPROTO;
	/* Platform pay_len includes the HCC header, unlike BLE pay_len. */
	record_len = get_unaligned_le16(data + 2);
	if (record_len < 18 || record_len > len)
		return -EMSGSIZE;
	if (get_unaligned_le16(data + 4) != 10) /* GET_CHIP_TYPE_COMPLETE */
		return -EPROTO;
	/* Status u32, returned length u32, package u8, bus u8. */
	message_len = get_unaligned_le16(data + 6);
	/* SDK110 replies can include unused allocation bytes in message_len. */
	if (message_len < 10 || message_len > record_len - 8 ||
	    get_unaligned_le32(data + 12) != 2)
		return -EMSGSIZE;
	if (get_unaligned_le32(data + 8))
		return -EREMOTEIO;
	parsed.package = data[16];
	parsed.bus = data[17];
	if (parsed.package > 2 || parsed.bus > 2)
		return -EOPNOTSUPP;

	*identity = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(btws73_chip_reply_decode, "WS73");

int ws73_version_query_encode(u8 *out, size_t capacity)
{
	if (!out)
		return -EINVAL;
	if (capacity < 32)
		return -ENOSPC;
	memset(out, 0, 32);
	out[0] = 0x19;
	out[1] = 9;
	put_unaligned_le16(32, out + 2);
	put_unaligned_le16(13, out + 4); /* GET_VERSION */
	put_unaligned_le16(1, out + 6);
	out[8] = 1;
	return 32;
}
EXPORT_SYMBOL_NS_GPL(ws73_version_query_encode, "WS73");

int ws73_version_reply_decode(const u8 *data, size_t len, struct ws73_version *version)
{
	struct ws73_version parsed;
	u16 record_len, message_len;

	if (!data || !version)
		return -EINVAL;
	if (len < 24 || len > BTWS73_RX_MAX_RECORD_SIZE)
		return -EMSGSIZE;
	if (data[0] != 0x19 || data[1] != 9 || get_unaligned_le16(data + 4) != 7)
		return -EPROTO;
	record_len = get_unaligned_le16(data + 2);
	message_len = get_unaligned_le16(data + 6);
	if (record_len < 24 || record_len > len ||
	    message_len < 16 || message_len > record_len - 8 ||
	    get_unaligned_le32(data + 12) != 8)
		return -EMSGSIZE;
	if (get_unaligned_le32(data + 8))
		return -EREMOTEIO;
	parsed.firmware = get_unaligned_le32(data + 16);
	parsed.rom = get_unaligned_le32(data + 20);
	/* Preserve all bits; a reported number is not firmware authentication. */
	*version = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_version_reply_decode, "WS73");

int btws73_hcc_decode(const u8 *data, size_t len, struct btws73_rx *rx)
{
	struct btws73_rx parsed = {};
	size_t offset = BTWS73_RX_HEADER_SIZE;
	bool ended = false;
	unsigned int i;

	if (!data || !rx)
		return -EINVAL;
	if (len < BTWS73_RX_HEADER_SIZE || len > BTWS73_RX_MAX_SIZE)
		return -EMSGSIZE;
	if (get_unaligned_le32(data) != BTWS73_USB_RX_AGGREGATE)
		return -EOPNOTSUPP;
	if (get_unaligned_le32(data + 4) != len)
		return -EMSGSIZE;
	if (get_unaligned_le32(data + 8) != BTWS73_BLE_QUEUE)
		return -EOPNOTSUPP;

	for (i = 0; i < BTWS73_RX_MAX_RECORDS; i++) {
		u16 size = get_unaligned_le16(data + 12 + 2 * i);
		const u8 *record;
		u16 payload;

		if (!size) {
			ended = true;
			continue;
		}
		if (ended)
			return -EPROTO;
		if (size < BTWS73_HCC_HEADER_SIZE ||
		    size > BTWS73_RX_MAX_RECORD_SIZE || size > len - offset)
			return -EMSGSIZE;

		record = data + offset;
		/* Service 0 / subtype 0, with matching outer and inner queue. */
		if (record[0] || record[1] != BTWS73_BLE_QUEUE)
			return -EPROTO;
		payload = get_unaligned_le16(record + 2);
		if (!payload || payload > BTWS73_H4_MAX_SIZE ||
		    payload > size - BTWS73_HCC_HEADER_SIZE)
			return -EMSGSIZE;

		parsed.fragment[parsed.count].data = record + BTWS73_HCC_HEADER_SIZE;
		parsed.fragment[parsed.count++].len = payload;
		offset += size;
	}

	if (!parsed.count || offset != len)
		return -EMSGSIZE;

	/* Publish only after checking every record, including the final one. */
	*rx = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(btws73_hcc_decode, "WS73");

MODULE_DESCRIPTION("WS73 shared protocol helpers");
MODULE_LICENSE("GPL");
