// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/unaligned.h>

#include "customize.h"
#include "usb-tx.h"

/* Deliberately synthetic endian markers: NOT a usable board/RF profile. */
static const struct ws73_customization ws73_custom_fixture = {
	.coexistence = 0x01020304,
	.disable_ll_privacy = 0x05060708,
	.suspend_mode = 0x090a0b0c,
	.scan_interval = 0x0d0e0f10,
	.scan_window = 0x11121314,
	.front_switch = 0x15161718,
	.max_power = -2,
	.calibration_count = 8,
	.calibration = {
		{ 0x2122, 0x2324 }, { 0x2526, 0x2728 },
		{ 0x292a, 0x2b2c }, { 0x2d2e, 0x2f30 },
		{ 0x3132, 0x3334 }, { 0x3536, 0x3738 },
		{ 0x393a, 0x3b3c }, { 0x3d3e, 0x3f40 },
	},
	.channel_boundary = { 0x4142, 0x4344, 0x4546, 0x4748, 0x494a, 0x4b4c, 0x4d4e },
	.srrc_switch = 0x51525354,
	.srrc = {
		{ 0x5556, 0x5758 }, { 0x595a, 0x5b5c },
		{ 0x5d5e, 0x5f60 }, { 0x6162, 0x6364 },
		{ 0x6566, 0x6768 }, { 0x696a, 0x6b6c },
		{ 0x6d6e, 0x6f70 }, { 0x7172, 0x7374 },
	},
};

static const u8 ws73_custom_wire[800] = {
	0x50, 0x0a, 0x20, 0x03, 0x00, 0x00, 0x8c, 0x00,
	0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05,
	0x0c, 0x0b, 0x0a, 0x09, 0x10, 0x0f, 0x0e, 0x0d,
	0x14, 0x13, 0x12, 0x11, 0x18, 0x17, 0x16, 0x15,
	0xfe, 0xff, 0xff, 0xff, 0x08, 0x00, 0x00, 0x00,
	0x24, 0x23, 0x22, 0x21, 0x28, 0x27, 0x26, 0x25,
	0x2c, 0x2b, 0x2a, 0x29, 0x30, 0x2f, 0x2e, 0x2d,
	0x34, 0x33, 0x32, 0x31, 0x38, 0x37, 0x36, 0x35,
	0x3c, 0x3b, 0x3a, 0x39, 0x40, 0x3f, 0x3e, 0x3d,
	0x44, 0x43, 0x42, 0x41, 0x48, 0x47, 0x46, 0x45,
	0x4c, 0x4b, 0x4a, 0x49, 0x4e, 0x4d, 0x00, 0x00,
	0x54, 0x53, 0x52, 0x51, 0x58, 0x57, 0x56, 0x55,
	0x5c, 0x5b, 0x5a, 0x59, 0x60, 0x5f, 0x5e, 0x5d,
	0x64, 0x63, 0x62, 0x61, 0x68, 0x67, 0x66, 0x65,
	0x6c, 0x6b, 0x6a, 0x69, 0x70, 0x6f, 0x6e, 0x6d,
	0x74, 0x73, 0x72, 0x71,
};

static void ws73_customize_wire_test(struct kunit *test)
{
	struct ws73_customization parameters = ws73_custom_fixture;
	u8 storage[802];
	u8 *out = storage + 1;
	unsigned int i;

	memset(storage, 0xa5, sizeof(storage));
	KUNIT_ASSERT_EQ(test, ws73_customize_encode(out, 800, &parameters), 800);
	KUNIT_EXPECT_MEMEQ(test, out, ws73_custom_wire, 800);
	KUNIT_EXPECT_EQ(test, storage[0], 0xa5);
	KUNIT_EXPECT_EQ(test, storage[801], 0xa5);
	KUNIT_EXPECT_EQ(test, ws73_customize_validate(out, 800), 0);
	for (i = 0; i <= 8; i++) {
		parameters.calibration_count = i;
		KUNIT_ASSERT_EQ(test, ws73_customize_encode(out, 800, &parameters), 800);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(out + 36), i);
		KUNIT_EXPECT_EQ(test, ws73_customize_validate(out, 800), 0);
	}
}

static void ws73_customize_encode_errors_test(struct kunit *test)
{
	struct ws73_customization parameters = ws73_custom_fixture;
	u8 out[801];
	unsigned int i;

	memset(out, 0xa5, sizeof(out));
	for (i = 0; i < 800; i++) {
		KUNIT_EXPECT_EQ(test, ws73_customize_encode(out, i, &parameters), -ENOSPC);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, sizeof(out)), NULL);
	}
	parameters.calibration_count = 9;
	KUNIT_EXPECT_EQ(test, ws73_customize_encode(out, 800, &parameters), -ERANGE);
	parameters.calibration_count = U32_MAX;
	KUNIT_EXPECT_EQ(test, ws73_customize_encode(out, 800, &parameters), -ERANGE);
	KUNIT_EXPECT_EQ(test, ws73_customize_encode(NULL, 800, &parameters), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_customize_encode(out, 800, NULL), -EINVAL);
	KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, sizeof(out)), NULL);
}

static void ws73_customize_validate_errors_test(struct kunit *test)
{
	u8 *out = kunit_kmalloc(test, 865, GFP_KERNEL);
	u8 record[801];
	unsigned int i;
	int expected;

	KUNIT_ASSERT_NOT_NULL(test, out);
	memcpy(record, ws73_custom_wire, 800);
	for (i = 0; i <= 801; i++) {
		if (i != 800)
			KUNIT_EXPECT_EQ(test, ws73_customize_validate(record, i), -EMSGSIZE);
	}
	KUNIT_EXPECT_EQ(test, ws73_customize_validate(record, SIZE_MAX), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, ws73_customize_validate(NULL, 800), -EINVAL);
	/* Reject every altered header byte and every forbidden override/tail byte. */
	for (i = 0; i < 800; i++) {
		if (i >= 8 && i < 124 && i != 36 && i != 86 && i != 87)
			continue;
		memcpy(record, ws73_custom_wire, 800);
		record[i] ^= 1;
		expected = i < 2 || i == 86 || i == 87 ? -EPROTO :
			(i == 2 || i == 3 || i == 6 || i == 7) ? -EMSGSIZE :
			i == 36 ? -ERANGE : -EOPNOTSUPP;
		KUNIT_EXPECT_EQ(test, ws73_customize_validate(record, 800), expected);
		memset(out, 0xa5, 865);
		KUNIT_EXPECT_LT(test, ws73_usb_single_encode(out, 864, 0, record, 800), 0);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, 865), NULL);
	}
	/* Recovery is a new valid input, not a modification of a failed output. */
	memcpy(record, ws73_custom_wire, 800);
	KUNIT_ASSERT_EQ(test, ws73_usb_single_encode(out, 864, 7, record, 800), 864);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(out + 4), 864);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(out + 8), 7);
	KUNIT_EXPECT_MEMEQ(test, out + 64, ws73_custom_wire, 800);
	KUNIT_EXPECT_EQ(test, out[864], 0xa5);
	/* A customization TX record is never accepted as a device status RX. */
	{
		enum ws73_notification notification = WS73_NOTIFY_BOOT;

		KUNIT_EXPECT_LT(test, ws73_notification_decode(record, 800, &notification), 0);
		KUNIT_EXPECT_EQ(test, notification, WS73_NOTIFY_BOOT);
	}
}

struct ws73_custom_tx_mock {
	struct kunit *test;
	unsigned int calls;
	unsigned int fail_at;
};

static u64 ws73_custom_now(void *context)
{
	return 0;
}

static bool ws73_custom_cancelled(void *context)
{
	return false;
}

static int ws73_custom_send(void *context, const u8 *data, size_t length,
			    unsigned int timeout)
{
	struct ws73_custom_tx_mock *mock = context;
	struct kunit *test = mock->test;

	KUNIT_EXPECT_EQ(test, timeout, 100);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 8), mock->calls);
	if (mock->calls < 2) {
		KUNIT_EXPECT_EQ(test, length, 36);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data), 3);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 12), 1 - mock->calls);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 16), mock->calls ? 0 : 10);
	} else {
		KUNIT_EXPECT_EQ(test, length, 864);
		KUNIT_EXPECT_MEMEQ(test, data + 64, ws73_custom_wire, 800);
	}
	mock->calls++;
	return mock->calls == mock->fail_at ? length - 1 : length;
}

static void ws73_customize_transport_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_custom_tx_mock mock = { .test = test };
	struct ws73_usb_tx_io io = {
		.context = &mock, .now_ms = ws73_custom_now,
		.cancelled = ws73_custom_cancelled, .send = ws73_custom_send,
	};
	u8 record[800];
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, tx);
	memcpy(record, ws73_custom_wire, 800);
	ws73_usb_tx_init(tx);
	record[124] = 1; /* No flash-override side effect is admitted. */
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, record, 800, 100), -EOPNOTSUPP);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	KUNIT_EXPECT_FALSE(test, tx->queue_valid);
	record[124] = 0;
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, record, 800, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	KUNIT_EXPECT_EQ(test, tx->queue, 10);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, record, 800, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 4);
	for (i = 1; i <= 3; i++) {
		ws73_usb_tx_init(tx);
		mock.calls = 0;
		mock.fail_at = i;
		KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, record, 800, 100), -EREMOTEIO);
		KUNIT_EXPECT_EQ(test, mock.calls, i);
		KUNIT_EXPECT_FALSE(test, tx->queue_valid);
		mock.fail_at = 0;
		KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, record, 800, 100), -EREMOTEIO);
		KUNIT_EXPECT_EQ(test, mock.calls, i);
	}
}

static struct kunit_case ws73_customize_cases[] = {
	KUNIT_CASE(ws73_customize_wire_test),
	KUNIT_CASE(ws73_customize_encode_errors_test),
	KUNIT_CASE(ws73_customize_validate_errors_test),
	KUNIT_CASE(ws73_customize_transport_test),
	{}
};

static struct kunit_suite ws73_customize_suite = {
	.name = "ws73-customize",
	.test_cases = ws73_customize_cases,
};

kunit_test_suite(ws73_customize_suite);
