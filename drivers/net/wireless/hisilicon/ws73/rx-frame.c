// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/unaligned.h>

#include "rx-frame.h"

int ws73_wifi_rx_frame_decode(const u8 *data, size_t length,
			      struct ws73_wifi_rx_frame *frame)
{
	const size_t prefix = WS73_WIFI_RX_CB_SIZE + WS73_WIFI_RX_HEADER_RESERVE;
	struct ws73_wifi_rx_frame parsed = {};
	size_t frame_length, available;

	if (!data || !frame)
		return -EINVAL;
	if (length < prefix || length > WS73_WIFI_RX_RECORD_MAX)
		return -EMSGSIZE;
	/* Split/AMSDU storage needs its own established reassembly contract. */
	if ((data[0] & 0x20) || (data[6] >> 4) != 1)
		return -EOPNOTSUPP;
	parsed.header_length = data[3] & 0x3f;
	frame_length = get_unaligned_le16(data + 4);
	if (!parsed.header_length || parsed.header_length > WS73_WIFI_RX_HEADER_RESERVE ||
	    frame_length < parsed.header_length)
		return -EMSGSIZE;
	parsed.body_length = frame_length - parsed.header_length;
	available = length - prefix;
	if (parsed.body_length > available || available - parsed.body_length > 31)
		return -EMSGSIZE;
	parsed.header = data + WS73_WIFI_RX_CB_SIZE;
	parsed.body = data + prefix;
	parsed.channel = data[7];
	parsed.signal = (s8)data[24];
	parsed.cipher = data[20] & 0x0f;
	parsed.status = data[20] >> 4;
	*frame = parsed;
	return 0;
}
