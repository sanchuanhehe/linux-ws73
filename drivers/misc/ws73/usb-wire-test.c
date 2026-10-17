// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "usb-wire.h"

static void ws73_usb_control_test(struct kunit *test)
{
	static const u8 switch_wire[36] = {
		3, 0, 0, 0, 0, 0, 0, 0, 0x78, 0x56, 0x34, 0x12,
		1, 0, 0, 0, 9,
	};
	static const u8 reset_wire[36] = {
		3, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
	};
	u8 out[37];
	unsigned int i;

	memset(out, 0xa5, sizeof(out));
	for (i = 0; i < 36; i++) {
		KUNIT_EXPECT_EQ(test, ws73_usb_control_encode(out, i, 0x12345678,
				WS73_USB_SWITCH_QUEUE, 9), -ENOSPC);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, sizeof(out)), NULL);
	}
	KUNIT_ASSERT_EQ(test, ws73_usb_control_encode(out, 36, 0x12345678,
			WS73_USB_SWITCH_QUEUE, 9), 36);
	KUNIT_EXPECT_MEMEQ(test, out, switch_wire, sizeof(switch_wire));
	KUNIT_EXPECT_EQ(test, out[36], 0xa5);
	KUNIT_ASSERT_EQ(test, ws73_usb_control_encode(out, 36, U32_MAX,
			WS73_USB_RESET_AGGREGATION, 7), 36);
	KUNIT_EXPECT_MEMEQ(test, out, reset_wire, sizeof(reset_wire));
	memset(out, 0xa5, sizeof(out));
	for (i = 0; i <= U8_MAX; i++) {
		if (i == 7 || i == 9 || i == 10)
			continue;
		KUNIT_EXPECT_EQ(test, ws73_usb_control_encode(out, 36, 0,
				WS73_USB_SWITCH_QUEUE, i), -EOPNOTSUPP);
	}
	KUNIT_EXPECT_EQ(test, ws73_usb_control_encode(out, 36, 0, 2, 7), -EOPNOTSUPP);
	KUNIT_EXPECT_EQ(test, ws73_usb_control_encode(out, 36, 0, -1, 7), -EOPNOTSUPP);
	KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, sizeof(out)), NULL);
	KUNIT_EXPECT_EQ(test, ws73_usb_control_encode(NULL, 36, 0, 0, 7), -EINVAL);
}

static void ws73_usb_single_test(struct kunit *test)
{
	static const u8 record[] = { 0, 7, 4, 0, 1, 3, 0x0c, 0 };
	static const u8 expected[128] = {
		[4] = 128, [8] = 0x78, [9] = 0x56, [10] = 0x34, [11] = 0x12,
		[65] = 7, [66] = 4, [68] = 1, [69] = 3, [70] = 0x0c,
	};
	u8 out[129], platform[32];
	unsigned int i;

	memset(out, 0xa5, sizeof(out));
	for (i = 0; i < 128; i++) {
		KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, i, 0,
				record, sizeof(record)), -ENOSPC);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, sizeof(out)), NULL);
	}
	KUNIT_ASSERT_EQ(test, ws73_usb_single_encode(out, 128, 0x12345678,
			record, sizeof(record)), 128);
	KUNIT_EXPECT_MEMEQ(test, out, expected, sizeof(expected));
	KUNIT_EXPECT_EQ(test, out[128], 0xa5);
	KUNIT_ASSERT_EQ(test, btws73_chip_query_encode(platform, sizeof(platform)), 32);
	KUNIT_ASSERT_EQ(test, ws73_usb_single_encode(out, 128, 0, platform, 32), 128);
	KUNIT_EXPECT_MEMEQ(test, out + 64, platform, sizeof(platform));
	KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out + 12, 0, 52), NULL);
	KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out + 96, 0, 32), NULL);
	KUNIT_EXPECT_EQ(test, out[128], 0xa5);
}

static void ws73_usb_single_limits_test(struct kunit *test)
{
	u8 *out = kunit_kmalloc(test, WS73_USB_TX_MAX + 1, GFP_KERNEL);
	u8 *record = kunit_kzalloc(test, 4097, GFP_KERNEL);
	unsigned int i;
	int expected;

	KUNIT_ASSERT_NOT_NULL(test, out);
	KUNIT_ASSERT_NOT_NULL(test, record);
	record[1] = 7;
	put_unaligned_le16(1, record + 2);
	for (i = 0; i <= 4097; i++) {
		memset(out, 0xa5, WS73_USB_TX_MAX + 1);
		expected = i < 8 || i > 4096 || (i & 7) ?
			-EMSGSIZE : 64 + max(i, 64U);
		KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, WS73_USB_TX_MAX,
				0, record, i), expected);
		if (expected < 0)
			KUNIT_EXPECT_PTR_EQ(test,
				memchr_inv(out, 0xa5, WS73_USB_TX_MAX + 1), NULL);
		else
			KUNIT_EXPECT_EQ(test, out[expected], 0xa5);
	}
	memset(out, 0xa5, WS73_USB_TX_MAX + 1);
	record[1] = 8;
	KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, WS73_USB_TX_MAX,
			0, record, 8), -EOPNOTSUPP);
	record[1] = 7;
	record[0] = 1;
	KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, WS73_USB_TX_MAX,
			0, record, 8), -EPROTO);
	KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, WS73_USB_TX_MAX,
			0, record, SIZE_MAX), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(NULL, 128, 0, record, 8), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, 128, 0, NULL, 8), -EINVAL);
	KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, WS73_USB_TX_MAX + 1), NULL);
}

static size_t ws73_usb_rx_fixture(u8 *data, unsigned int count, u8 queue, u16 size)
{
	size_t length = 92 + count * size;
	unsigned int i;

	memset(data, 0, length);
	put_unaligned_le32(2, data);
	put_unaligned_le32(length, data + 4);
	put_unaligned_le32(queue, data + 8);
	for (i = 0; i < count; i++) {
		u8 *record = data + 92 + i * size;

		put_unaligned_le16(size, data + 12 + 2 * i);
		record[0] = queue == 9 ? 0x19 : 0;
		record[1] = queue;
		put_unaligned_le16(queue == 9 ? 18 : 7, record + 2);
		if (queue == 9) {
			/* Synthetic chip reply, not a device completion. */
			put_unaligned_le16(10, record + 4);
			put_unaligned_le16(10, record + 6);
			put_unaligned_le32(2, record + 12);
		}
	}
	return length;
}

static void ws73_usb_aggregate_test(struct kunit *test)
{
	u8 *storage = kunit_kmalloc(test, BTWS73_RX_MAX_SIZE + 1, GFP_KERNEL);
	struct ws73_usb_rx rx;
	struct btws73_chip_identity chip;
	unsigned int count, i, queue;
	size_t length;
	u8 *data;

	KUNIT_ASSERT_NOT_NULL(test, storage);
	data = storage + 1; /* Unaligned USB input. */
	for (queue = 7; queue <= 9; queue += 2) {
		for (count = 1; count <= 24; count++) {
			length = ws73_usb_rx_fixture(data, count, queue, 4096);
			KUNIT_ASSERT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), 0);
			KUNIT_EXPECT_EQ(test, rx.queue, queue);
			KUNIT_EXPECT_EQ(test, rx.count, count);
			for (i = 0; i < count; i++) {
				KUNIT_EXPECT_PTR_EQ(test, rx.record[i].data, data + 92 + i * 4096);
				KUNIT_EXPECT_EQ(test, rx.record[i].len, 4096);
				if (queue == 9)
					KUNIT_EXPECT_EQ(test, btws73_chip_reply_decode(
						rx.record[i].data, rx.record[i].len, &chip), 0);
			}
		}
	}
}

static void ws73_usb_aggregate_errors_test(struct kunit *test)
{
	u8 data[92 + 64];
	struct ws73_usb_rx rx, before;
	size_t length = ws73_usb_rx_fixture(data, 2, 9, 32);
	unsigned int i;

	memset(&before, 0xa5, sizeof(before));
	for (i = 0; i < length; i++) {
		rx = before;
		KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, i, &rx), -EMSGSIZE);
		KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
		put_unaligned_le32(i, data + 4);
		KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, i, &rx), -EMSGSIZE);
		KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
		put_unaligned_le32(length, data + 4);
	}
	for (i = 0; i <= U16_MAX; i++) {
		rx = before;
		put_unaligned_le16(i, data + 92 + 32 + 2);
		KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx),
				i >= 18 && i <= 32 ? 0 : -EMSGSIZE);
		if (i < 18 || i > 32)
			KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
	}
	ws73_usb_rx_fixture(data, 2, 9, 32);
	for (i = 0; i < 32; i++) {
		put_unaligned_le32(9 | BIT(i), data + 8);
		if (i == 0 || i == 3)
			continue;
		rx = before;
		KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EOPNOTSUPP);
		KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
	}
	ws73_usb_rx_fixture(data, 2, 9, 32);
	rx = before;
	data[92 + 32] = 0;
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EPROTO);
	data[92 + 32] = 0x19;
	data[92 + 32 + 1] = 7;
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EPROTO);
	data[92 + 32 + 1] = 9;
	put_unaligned_le16(0, data + 12);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EPROTO);
	put_unaligned_le16(32, data + 12);
	put_unaligned_le16(0, data + 14);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EMSGSIZE);
	put_unaligned_le16(4097, data + 14);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EMSGSIZE);
	put_unaligned_le32(0x102, data);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EOPNOTSUPP);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, BTWS73_RX_MAX_SIZE + 1,
			&rx), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(NULL, length, &rx), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, NULL), -EINVAL);
	KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
	length = ws73_usb_rx_fixture(data, 0, 9, 32);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EMSGSIZE);
	length = ws73_usb_rx_fixture(data, 2, 9, 32);
	KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), 0);
}

static struct kunit_case ws73_usb_wire_cases[] = {
	KUNIT_CASE(ws73_usb_control_test),
	KUNIT_CASE(ws73_usb_single_test),
	KUNIT_CASE(ws73_usb_single_limits_test),
	KUNIT_CASE(ws73_usb_aggregate_test),
	KUNIT_CASE(ws73_usb_aggregate_errors_test),
	{}
};

static struct kunit_suite ws73_usb_wire_suite = {
	.name = "ws73-usb-wire",
	.test_cases = ws73_usb_wire_cases,
};

kunit_test_suite(ws73_usb_wire_suite);
