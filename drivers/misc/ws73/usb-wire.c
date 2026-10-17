// SPDX-License-Identifier: GPL-2.0-only
/* SDK110 USB wire format; independent implementation, no vendor source. */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include "usb-wire.h"

static bool ws73_usb_queue_valid(u32 queue)
{
	return queue == 7 || queue == 9;
}

static int ws73_usb_record_validate(const u8 *record, size_t length, u32 queue)
{
	u16 payload;

	if (length < BTWS73_HCC_HEADER_SIZE ||
	    length > BTWS73_RX_MAX_RECORD_SIZE)
		return -EMSGSIZE;
	if (!ws73_usb_queue_valid(queue))
		return -EOPNOTSUPP;
	if (record[1] != queue)
		return -EPROTO;
	payload = get_unaligned_le16(record + 2);
	if (queue == 7) {
		if (record[0])
			return -EPROTO;
		if (!payload || payload > BTWS73_H4_MAX_SIZE || payload > length - 4)
			return -EMSGSIZE;
	} else {
		/* Platform payload length includes HCC, unlike the BLE service. */
		if (record[0] != 0x19)
			return -EPROTO;
		if (payload < 8 || payload > length)
			return -EMSGSIZE;
		if (get_unaligned_le16(record + 6) > payload - 8)
			return -EMSGSIZE;
	}
	return 0;
}

int ws73_usb_control_encode(u8 *out, size_t capacity, u32 sequence,
			    enum ws73_usb_control operation, u8 queue)
{
	if (!out)
		return -EINVAL;
	if (operation != WS73_USB_RESET_AGGREGATION &&
	    operation != WS73_USB_SWITCH_QUEUE)
		return -EOPNOTSUPP;
	if (!ws73_usb_queue_valid(queue))
		return -EOPNOTSUPP;
	if (capacity < WS73_USB_CONTROL_SIZE)
		return -ENOSPC;

	memset(out, 0, WS73_USB_CONTROL_SIZE);
	put_unaligned_le32(3, out);
	/* Control length field is zero, although the USB transfer is 36 bytes. */
	put_unaligned_le32(sequence, out + 8);
	put_unaligned_le32(operation, out + 12);
	if (operation == WS73_USB_SWITCH_QUEUE)
		put_unaligned_le32(queue, out + 16);
	return WS73_USB_CONTROL_SIZE;
}
EXPORT_SYMBOL_NS_GPL(ws73_usb_control_encode, "WS73");

int ws73_usb_single_encode(u8 *out, size_t capacity, u32 sequence,
			   const u8 *record, size_t length)
{
	size_t total;
	int ret;

	if (!out || !record)
		return -EINVAL;
	if (length < 4 || length > BTWS73_RX_MAX_RECORD_SIZE || (length & 7))
		return -EMSGSIZE;
	ret = ws73_usb_record_validate(record, length, record[1]);
	if (ret)
		return ret;
	total = WS73_USB_TX_PREFIX + max_t(size_t, length, 64);
	if (capacity < total)
		return -ENOSPC;

	memset(out, 0, total);
	/* Type 0: one HCC record after a zero descriptor and 64-byte prefix. */
	put_unaligned_le32(total, out + 4);
	put_unaligned_le32(sequence, out + 8);
	memcpy(out + WS73_USB_TX_PREFIX, record, length);
	return total;
}
EXPORT_SYMBOL_NS_GPL(ws73_usb_single_encode, "WS73");

int ws73_usb_aggregate_decode(const u8 *data, size_t length,
			      struct ws73_usb_rx *rx)
{
	struct ws73_usb_rx parsed = {};
	size_t offset = BTWS73_RX_HEADER_SIZE;
	bool ended = false;
	u32 queue;
	unsigned int i;
	int ret;

	if (!data || !rx)
		return -EINVAL;
	if (length < BTWS73_RX_HEADER_SIZE || length > BTWS73_RX_MAX_SIZE)
		return -EMSGSIZE;
	if (get_unaligned_le32(data) != 2)
		return -EOPNOTSUPP;
	if (get_unaligned_le32(data + 4) != length)
		return -EMSGSIZE;
	queue = get_unaligned_le32(data + 8);
	if (!ws73_usb_queue_valid(queue))
		return -EOPNOTSUPP;
	parsed.queue = queue;
	for (i = 0; i < BTWS73_RX_MAX_RECORDS; i++) {
		u16 size = get_unaligned_le16(data + 12 + 2 * i);

		if (!size) {
			ended = true;
			continue;
		}
		if (ended)
			return -EPROTO;
		if (size > length - offset)
			return -EMSGSIZE;
		ret = ws73_usb_record_validate(data + offset, size, queue);
		if (ret)
			return ret;
		parsed.record[parsed.count].data = data + offset;
		parsed.record[parsed.count++].len = size;
		offset += size;
	}
	if (!parsed.count || offset != length)
		return -EMSGSIZE;
	*rx = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_usb_aggregate_decode, "WS73");
