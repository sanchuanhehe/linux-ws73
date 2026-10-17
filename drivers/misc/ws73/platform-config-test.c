// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/unaligned.h>

#include "platform-config.h"

static void ws73_platform_config_wire_test(struct kunit *test)
{
	/* GPIO 0xa5, active-high wake and link checking, with sleep disabled. */
	const u8 expected[32] = { 0x19, 9, 32, 0, 0, 0, 4, 0, 0x4a, 7 };
	struct ws73_platform_config config = { 0xa5, true, true };
	u8 storage[34];
	unsigned int i;

	memset(storage, 0xa5, sizeof(storage));
	for (i = 0; i < 32; i++) {
		KUNIT_EXPECT_EQ(test, ws73_platform_config_encode(storage + 1, i, &config),
				-ENOSPC);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(storage, 0xa5, sizeof(storage)), NULL);
	}
	KUNIT_ASSERT_EQ(test, ws73_platform_config_encode(storage + 1, 32, &config), 32);
	KUNIT_EXPECT_MEMEQ(test, storage + 1, expected, sizeof(expected));
	KUNIT_EXPECT_EQ(test, storage[0], 0xa5);
	KUNIT_EXPECT_EQ(test, storage[33], 0xa5);
	config.wake_gpio = 256;
	KUNIT_EXPECT_EQ(test, ws73_platform_config_encode(storage + 1, 32, &config), -ERANGE);
	KUNIT_EXPECT_MEMEQ(test, storage + 1, expected, sizeof(expected));
	KUNIT_EXPECT_EQ(test, ws73_platform_config_encode(storage, 32, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_platform_config_encode(NULL, 32, &config), -EINVAL);
}

struct ws73_config_mock {
	struct kunit *test;
	unsigned int calls, fail;
	u64 now;
	bool cancelled;
};

static u64 ws73_config_now(void *context)
{
	return ((struct ws73_config_mock *)context)->now;
}

static bool ws73_config_cancelled(void *context)
{
	return ((struct ws73_config_mock *)context)->cancelled;
}

static int ws73_config_send(void *context, const u8 *data, size_t size, unsigned int timeout)
{
	struct ws73_config_mock *mock = context;
	const u8 payload[64] = { 0x19, 9, 32, 0, 0, 0, 4, 0, 0xfe, 1 };

	mock->calls++;
	KUNIT_EXPECT_EQ(mock->test, timeout, 100);
	if (mock->calls == 3) {
		KUNIT_EXPECT_EQ(mock->test, size, 128);
		KUNIT_EXPECT_MEMEQ(mock->test, data + 64, payload, sizeof(payload));
	}
	return mock->calls == mock->fail ? size - 1 : size;
}

static void ws73_platform_config_transport_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_platform_config config = { .wake_gpio = 255 };
	struct ws73_config_mock mock = { .test = test };
	struct ws73_usb_tx_io io = {
		.context = &mock, .now_ms = ws73_config_now,
		.cancelled = ws73_config_cancelled, .send = ws73_config_send,
	};
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, tx);
	for (i = 0; i <= 3; i++) {
		ws73_usb_tx_init(tx);
		mock.calls = 0;
		mock.fail = i;
		KUNIT_EXPECT_EQ(test, ws73_platform_configure(tx, &io, &config, 100),
				i ? -EREMOTEIO : 0);
		KUNIT_EXPECT_EQ(test, mock.calls, i ?: 3);
		if (i) {
			KUNIT_EXPECT_EQ(test, ws73_platform_configure(tx, &io, &config, 100),
					-EREMOTEIO);
			KUNIT_EXPECT_EQ(test, mock.calls, i);
		}
	}
	ws73_usb_tx_init(tx);
	mock.calls = 0;
	config.wake_gpio = 256;
	KUNIT_EXPECT_EQ(test, ws73_platform_configure(tx, &io, &config, 100), -ERANGE);
	config.wake_gpio = 10;
	mock.now = 100;
	KUNIT_EXPECT_EQ(test, ws73_platform_configure(tx, &io, &config, 100), -ETIMEDOUT);
	mock.now = 0;
	mock.cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_platform_configure(tx, &io, &config, 100), -ENODEV);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
}

static struct kunit_case ws73_platform_config_cases[] = {
	KUNIT_CASE(ws73_platform_config_wire_test),
	KUNIT_CASE(ws73_platform_config_transport_test),
	{}
};

static struct kunit_suite ws73_platform_config_suite = {
	.name = "ws73-platform-config",
	.test_cases = ws73_platform_config_cases,
};
kunit_test_suite(ws73_platform_config_suite);
MODULE_LICENSE("GPL");
