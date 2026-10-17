// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/unaligned.h>

#include "rx-frame.h"

/* Independent synthetic CB, header reservation and body, not a device packet. */
static void make_frame(u8 *data, size_t length)
{
	memset(data, 0, length);
	data[3] = 24;
	put_unaligned_le16(28, data + 4);
	data[6] = 0x10;
	data[7] = 11;
	memset(data + 16, 0xff, 4); /* Host must never dereference the device address. */
	data[20] = 0xa5;
	data[24] = 0x80;
	memset(data + 32, 0x12, 24);
	memset(data + 56, 0xee, 12); /* Not part of the MAC header or body. */
	memcpy(data + 68, "body", 4);
}

static void expect_error(struct kunit *test, const u8 *data, size_t length, int error)
{
	struct ws73_wifi_rx_frame frame;
	u8 before[sizeof(frame)];

	memset(&frame, 0xa5, sizeof(frame));
	memcpy(before, &frame, sizeof(frame));
	KUNIT_EXPECT_EQ(test, ws73_wifi_rx_frame_decode(data, length, &frame), error);
	KUNIT_EXPECT_MEMEQ(test, &frame, before, sizeof(frame));
}

static void ws73_wifi_rx_spans(struct kunit *test)
{
	struct ws73_wifi_rx_frame frame;
	u8 bytes[73];

	make_frame(bytes + 1, 72);
	KUNIT_ASSERT_EQ(test, ws73_wifi_rx_frame_decode(bytes + 1, 72, &frame), 0);
	KUNIT_EXPECT_PTR_EQ(test, frame.header, bytes + 33);
	KUNIT_EXPECT_PTR_EQ(test, frame.body, bytes + 69);
	KUNIT_EXPECT_EQ(test, frame.header_length, 24);
	KUNIT_EXPECT_EQ(test, frame.body_length, 4);
	KUNIT_EXPECT_MEMEQ(test, frame.body, "body", 4);
	KUNIT_EXPECT_EQ(test, frame.channel, 11);
	KUNIT_EXPECT_EQ(test, frame.signal, -128);
	KUNIT_EXPECT_EQ(test, frame.cipher, 5);
	KUNIT_EXPECT_EQ(test, frame.status, 10);
}

static void ws73_wifi_rx_truncation(struct kunit *test)
{
	u8 original[72];
	size_t i;

	make_frame(original, sizeof(original));
	for (i = 0; i < sizeof(original); i++) {
		u8 *copy = kunit_kmalloc(test, i + 1, GFP_KERNEL);

		KUNIT_ASSERT_NOT_NULL(test, copy);
		memcpy(copy + 1, original, i);
		expect_error(test, copy + 1, i, -EMSGSIZE);
		kunit_kfree(test, copy);
	}
}

static void ws73_wifi_rx_lengths(struct kunit *test)
{
	struct ws73_wifi_rx_frame frame;
	u8 bytes[104];
	unsigned int i;

	for (i = 0; i < 64; i++) {
		make_frame(bytes, sizeof(bytes));
		bytes[3] = i;
		put_unaligned_le16(i + 4, bytes + 4);
		if (i && i <= 36) {
			KUNIT_ASSERT_EQ(test, ws73_wifi_rx_frame_decode(bytes, 72, &frame), 0);
			KUNIT_EXPECT_EQ(test, frame.header_length, i);
			KUNIT_EXPECT_EQ(test, frame.body_length, 4);
		} else {
			expect_error(test, bytes, 72, -EMSGSIZE);
		}
	}
	make_frame(bytes, sizeof(bytes));
	put_unaligned_le16(23, bytes + 4);
	expect_error(test, bytes, 72, -EMSGSIZE);
	put_unaligned_le16(24, bytes + 4);
	KUNIT_ASSERT_EQ(test, ws73_wifi_rx_frame_decode(bytes, 68, &frame), 0);
	KUNIT_EXPECT_EQ(test, frame.body_length, 0);
	make_frame(bytes, sizeof(bytes));
	for (i = 0; i <= 31; i++)
		KUNIT_EXPECT_EQ(test, ws73_wifi_rx_frame_decode(bytes, 72 + i, &frame), 0);
	expect_error(test, bytes, 104, -EMSGSIZE);
}

static void ws73_wifi_rx_unsupported(struct kunit *test)
{
	struct ws73_wifi_rx_frame frame;
	u8 bytes[72];
	unsigned int i;

	for (i = 0; i < 16; i++) {
		make_frame(bytes, sizeof(bytes));
		bytes[6] = (i << 4) | 0x0f;
		if (i == 1)
			KUNIT_EXPECT_EQ(test, ws73_wifi_rx_frame_decode(bytes, 72, &frame), 0);
		else
			expect_error(test, bytes, 72, -EOPNOTSUPP);
	}
	make_frame(bytes, sizeof(bytes));
	bytes[0] |= 0x20;
	expect_error(test, bytes, sizeof(bytes), -EOPNOTSUPP);
	/* A rejected record cannot poison the following independent frame. */
	bytes[0] = 0;
	KUNIT_EXPECT_EQ(test, ws73_wifi_rx_frame_decode(bytes, 72, &frame), 0);
}

static void ws73_wifi_rx_limits(struct kunit *test)
{
	struct ws73_wifi_rx_frame frame;
	u8 *bytes = kunit_kmalloc(test, WS73_WIFI_RX_RECORD_MAX, GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, bytes);
	make_frame(bytes, WS73_WIFI_RX_RECORD_MAX);
	put_unaligned_le16(WS73_WIFI_RX_RECORD_MAX - 68 + 24, bytes + 4);
	KUNIT_EXPECT_EQ(test, ws73_wifi_rx_frame_decode(bytes, WS73_WIFI_RX_RECORD_MAX, &frame), 0);
	KUNIT_EXPECT_EQ(test, frame.body_length, WS73_WIFI_RX_RECORD_MAX - 68);
	put_unaligned_le16(U16_MAX, bytes + 4);
	expect_error(test, bytes, WS73_WIFI_RX_RECORD_MAX, -EMSGSIZE);
	expect_error(test, bytes, WS73_WIFI_RX_RECORD_MAX + 1, -EMSGSIZE);
	expect_error(test, bytes, SIZE_MAX, -EMSGSIZE);
	expect_error(test, NULL, 72, -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_wifi_rx_frame_decode(bytes, 72, NULL), -EINVAL);
}

static struct kunit_case ws73_wifi_rx_cases[] = {
	KUNIT_CASE(ws73_wifi_rx_spans),
	KUNIT_CASE(ws73_wifi_rx_truncation),
	KUNIT_CASE(ws73_wifi_rx_lengths),
	KUNIT_CASE(ws73_wifi_rx_unsupported),
	KUNIT_CASE(ws73_wifi_rx_limits),
	{}
};

static struct kunit_suite ws73_wifi_rx_suite = {
	.name = "ws73-wifi-rx-frame",
	.test_cases = ws73_wifi_rx_cases,
};

kunit_test_suite(ws73_wifi_rx_suite);
MODULE_LICENSE("GPL");
