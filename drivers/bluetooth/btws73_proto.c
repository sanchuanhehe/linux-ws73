// SPDX-License-Identifier: GPL-2.0-only
/* WS73 BLE wire validation. See Documentation/driver-api/ws73.rst. */

#include <linux/align.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>

#include "btws73_proto.h"

#define BTWS73_USB_RX_AGGREGATE	2
#define BTWS73_BLE_QUEUE		7
#define BTWS73_TX_ALIGNMENT	8

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

int btws73_chip_reply_decode(const u8 *data, size_t len,
			     struct btws73_chip_identity *identity)
{
	struct btws73_chip_identity parsed;
	u16 record_len;

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
	if (get_unaligned_le16(data + 6) != 10 ||
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

int btws73_hci_validate(const u8 *data, size_t len, bool tx)
{
	size_t header, payload;

	if (!data)
		return -EINVAL;
	if (!len || len > BTWS73_H4_MAX_SIZE)
		return -EMSGSIZE;

	switch (data[0]) {
	case HCI_COMMAND_PKT:
		if (!tx)
			return -EOPNOTSUPP;
		header = 1 + HCI_COMMAND_HDR_SIZE;
		if (len < header)
			return -EMSGSIZE;
		payload = data[3];
		break;
	case HCI_EVENT_PKT:
		if (tx)
			return -EOPNOTSUPP;
		header = 1 + HCI_EVENT_HDR_SIZE;
		if (len < header)
			return -EMSGSIZE;
		payload = data[2];
		break;
	case HCI_ACLDATA_PKT:
		header = 1 + HCI_ACL_HDR_SIZE;
		if (len < header)
			return -EMSGSIZE;
		payload = get_unaligned_le16(data + 3);
		break;
	default:
		/* SCO and ISO are outside this initial BLE transport subset. */
		return -EOPNOTSUPP;
	}

	return payload == len - header ? 0 : -EMSGSIZE;
}

int btws73_hcc_encode(u8 *out, size_t capacity, const u8 *h4, size_t len)
{
	size_t size;
	int err;

	if (!out)
		return -EINVAL;
	err = btws73_hci_validate(h4, len, true);
	if (err)
		return err;

	size = ALIGN(BTWS73_HCC_HEADER_SIZE + len, BTWS73_TX_ALIGNMENT);
	if (size > capacity)
		return -ENOSPC;

	memset(out, 0, size);
	out[1] = BTWS73_BLE_QUEUE;
	put_unaligned_le16(len, out + 2);
	memcpy(out + BTWS73_HCC_HEADER_SIZE, h4, len);
	return size;
}
