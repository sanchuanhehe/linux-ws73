// SPDX-License-Identifier: GPL-2.0-only
/* WS73 BLE wire validation. See Documentation/driver-api/ws73.rst. */

#include <linux/align.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>

#include "btws73_proto.h"

#define BTWS73_BLE_QUEUE		7
#define BTWS73_TX_ALIGNMENT	8

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

void btws73_h4_reset(struct btws73_h4 *stream)
{
	memset(stream, 0, sizeof(*stream));
}

int btws73_h4_feed(struct btws73_h4 *stream, const u8 *data, size_t len,
		   size_t *consumed, const u8 **packet, size_t *packet_len)
{
	size_t used = 0, target, copy;
	int ret;

	if (!stream || (!data && len) || !consumed || !packet || !packet_len)
		return -EINVAL;
	*consumed = 0;
	if (stream->failed)
		return -ESHUTDOWN;

	for (;;) {
		target = 1;
		if (stream->len) {
			switch (stream->data[0]) {
			case HCI_EVENT_PKT:
				target = 1 + HCI_EVENT_HDR_SIZE;
				if (stream->len >= target)
					target += stream->data[2];
				break;
			case HCI_ACLDATA_PKT:
				target = 1 + HCI_ACL_HDR_SIZE;
				if (stream->len >= target)
					target += get_unaligned_le16(stream->data + 3);
				break;
			default:
				ret = -EOPNOTSUPP;
				goto fail;
			}
		}
		if (target > sizeof(stream->data)) {
			ret = -EMSGSIZE;
			goto fail;
		}
		if (stream->len == target) {
			ret = btws73_hci_validate(stream->data, target, false);
			if (ret)
				goto fail;
			*packet = stream->data;
			*packet_len = target;
			stream->len = 0;
			*consumed = used;
			return 1;
		}
		if (used == len) {
			*consumed = used;
			return 0;
		}
		copy = min(target - stream->len, len - used);
		memcpy(stream->data + stream->len, data + used, copy);
		stream->len += copy;
		used += copy;
	}

fail:
	stream->failed = true;
	*consumed = used;
	return ret;
}

EXPORT_SYMBOL_NS_GPL(btws73_hci_validate, "BTWS73");
EXPORT_SYMBOL_NS_GPL(btws73_hcc_encode, "BTWS73");
EXPORT_SYMBOL_NS_GPL(btws73_h4_reset, "BTWS73");
EXPORT_SYMBOL_NS_GPL(btws73_h4_feed, "BTWS73");
MODULE_DESCRIPTION("WS73 BLE H4 codec");
MODULE_LICENSE("GPL");
