// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "usb-wire.h"

static void ws73_notification_fixture(u8 *record, u16 tag, u32 value)
{
	memset(record, 0, 16);
	record[0] = 5;
	record[1] = 10;
	put_unaligned_le16(12, record + 2);
	put_unaligned_le16(tag, record + 4);
	put_unaligned_le16(4, record + 6);
	put_unaligned_le32(value, record + 8);
}

static void ws73_notification_valid_test(struct kunit *test)
{
	static const u8 wire[] = { 5, 10, 12, 0, 4, 0, 4, 0, 0, 0, 0, 0 };
	enum ws73_notification event;
	u8 storage[17], *record = storage + 1;
	unsigned int i;

	KUNIT_ASSERT_EQ(test, ws73_notification_decode(wire, sizeof(wire), &event), 0);
	KUNIT_EXPECT_EQ(test, event, WS73_NOTIFY_BLE_OPEN);
	for (i = 0; i < 4; i++) {
		ws73_notification_fixture(record, i < 2 ? 2 : 4, i < 2 ? i + 1 : i - 2);
		KUNIT_ASSERT_EQ(test, ws73_notification_decode(record, 12, &event), 0);
		KUNIT_EXPECT_EQ(test, event, i);
		memset(record + 12, 0xa5, 4); /* Transport padding is not a new message. */
		KUNIT_ASSERT_EQ(test, ws73_notification_decode(record, 16, &event), 0);
		KUNIT_EXPECT_EQ(test, event, i);
		put_unaligned_le16(16, record + 2);
		KUNIT_ASSERT_EQ(test, ws73_notification_decode(record, 16, &event), 0);
		KUNIT_EXPECT_EQ(test, event, i);
	}
}

static void ws73_notification_invalid_test(struct kunit *test)
{
	enum ws73_notification event;
	u8 record[16];
	unsigned int i;
	int ret;

	ws73_notification_fixture(record, 2, 1);
	for (i = 0; i < 12; i++) {
		event = 99;
		KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, i, &event), -EMSGSIZE);
		KUNIT_EXPECT_EQ(test, event, 99);
	}
	for (i = 0; i <= U16_MAX; i++) {
		event = 99;
		put_unaligned_le16(i, record + 2);
		ret = ws73_notification_decode(record, sizeof(record), &event);
		KUNIT_EXPECT_EQ(test, ret, i >= 12 && i <= 16 ? 0 : -EMSGSIZE);
		if (ret)
			KUNIT_EXPECT_EQ(test, event, 99);
	}
	put_unaligned_le16(12, record + 2);
	for (i = 0; i <= U16_MAX; i++) {
		event = 99;
		put_unaligned_le16(i, record + 6);
		ret = ws73_notification_decode(record, sizeof(record), &event);
		KUNIT_EXPECT_EQ(test, ret, i == 4 ? 0 : -EMSGSIZE);
		if (ret)
			KUNIT_EXPECT_EQ(test, event, 99);
	}
	ws73_notification_fixture(record, 2, 1);
	for (i = 0; i <= U8_MAX; i++) {
		if (i == 5)
			continue;
		event = 99;
		record[0] = i;
		KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event), -EPROTO);
		KUNIT_EXPECT_EQ(test, event, 99);
	}
	record[0] = 5;
	record[1] = 9;
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event), -EPROTO);
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, SIZE_MAX, &event), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(NULL, 16, &event), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, event, 99);
}

static void ws73_notification_values_test(struct kunit *test)
{
	enum ws73_notification event;
	u8 record[16];
	unsigned int tag, value;
	int expected;

	for (tag = 0; tag < 8; tag++) {
		for (value = 0; value < 8; value++) {
			ws73_notification_fixture(record, tag, value);
			event = 99;
			expected = -EOPNOTSUPP;
			if (tag == 2)
				expected = value == 1 || value == 2 ? 0 : -EPROTO;
			if (tag == 4)
				expected = value < 2 ? 0 : value < 4 ? -EOPNOTSUPP : -EPROTO;
			KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event),
					expected);
			if (expected)
				KUNIT_EXPECT_EQ(test, event, 99);
		}
	}
	for (tag = 2; tag <= 4; tag += 2) {
		for (value = 2; value < 32; value++) {
			event = 99;
			ws73_notification_fixture(record, tag, BIT(value));
			KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event),
					-EPROTO);
			KUNIT_EXPECT_EQ(test, event, 99);
		}
	}
	ws73_notification_fixture(record, U16_MAX, U32_MAX);
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event), -EOPNOTSUPP);
	/* Unknown messages must still have a valid envelope. */
	put_unaligned_le16(5, record + 6);
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event), -EMSGSIZE);
	/* A preceding error does not poison the pure decoder. */
	ws73_notification_fixture(record, 2, 1);
	KUNIT_EXPECT_EQ(test, ws73_notification_decode(record, 16, &event), 0);
	KUNIT_EXPECT_EQ(test, event, WS73_NOTIFY_BOOT);
}

static void ws73_notification_aggregate_test(struct kunit *test)
{
	struct ws73_usb_rx rx, before;
	u8 data[92 + 24 * 16] = {}, out[128];
	unsigned int count, i;
	size_t length;

	memset(&before, 0xa5, sizeof(before));
	for (count = 1; count <= 24; count++) {
		memset(data, 0, sizeof(data));
		length = 92 + count * 16;
		put_unaligned_le32(2, data);
		put_unaligned_le32(length, data + 4);
		put_unaligned_le32(10, data + 8);
		for (i = 0; i < count; i++) {
			put_unaligned_le16(16, data + 12 + i * 2);
			ws73_notification_fixture(data + 92 + 16 * i, 4, i & 1);
		}
		KUNIT_ASSERT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), 0);
		KUNIT_EXPECT_EQ(test, rx.count, count);
		KUNIT_EXPECT_EQ(test, rx.queue, 10);
		for (i = 0; i < count; i++)
			KUNIT_EXPECT_EQ(test, rx.notification[i],
					i & 1 ? WS73_NOTIFY_BLE_CLOSE : WS73_NOTIFY_BLE_OPEN);
		/* The last invalid record prevents publication of earlier valid ones. */
		put_unaligned_le32(U32_MAX, data + 92 + 16 * (count - 1) + 8);
		rx = before;
		KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EPROTO);
		KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
		/* SLE is unsupported, and also cannot publish a partial aggregate. */
		put_unaligned_le32(2, data + 92 + 16 * (count - 1) + 8);
		KUNIT_EXPECT_EQ(test, ws73_usb_aggregate_decode(data, length, &rx), -EOPNOTSUPP);
		KUNIT_EXPECT_MEMEQ(test, &rx, &before, sizeof(rx));
	}
	/* RX admission does not enable unchecked queue-10 TX. */
	KUNIT_EXPECT_EQ(test, ws73_usb_single_encode(out, sizeof(out), 0,
			data + 92, 16), -EOPNOTSUPP);
	KUNIT_EXPECT_EQ(test, ws73_usb_control_encode(out, sizeof(out), 0,
			WS73_USB_SWITCH_QUEUE, 10), -EOPNOTSUPP);
}

static struct kunit_case ws73_service_cases[] = {
	KUNIT_CASE(ws73_notification_valid_test),
	KUNIT_CASE(ws73_notification_invalid_test),
	KUNIT_CASE(ws73_notification_values_test),
	KUNIT_CASE(ws73_notification_aggregate_test),
	{}
};

static struct kunit_suite ws73_service_suite = {
	.name = "ws73-service",
	.test_cases = ws73_service_cases,
};
kunit_test_suite(ws73_service_suite);

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
