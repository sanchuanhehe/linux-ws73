// SPDX-License-Identifier: GPL-2.0-only
/* WS73 BLE wire validation. See Documentation/driver-api/ws73.rst. */

#include <linux/align.h>
#include <linux/errno.h>
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
