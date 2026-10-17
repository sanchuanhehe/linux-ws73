// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/unaligned.h>

#include "usb-tx.h"

struct ws73_tx_mock {
	struct kunit *test;
	u64 now;
	u64 deadline;
	u32 sequence;
	unsigned int calls;
	unsigned int fail_at;
	unsigned int fault;
	u8 queue;
	u8 step;
	bool cancelled;
};

static u64 ws73_tx_now(void *context)
{
	return ((struct ws73_tx_mock *)context)->now;
}

static bool ws73_tx_cancelled(void *context)
{
	return ((struct ws73_tx_mock *)context)->cancelled;
}

static int ws73_tx_send(void *context, const u8 *data, size_t length, unsigned int timeout)
{
	struct ws73_tx_mock *mock = context;
	struct kunit *test = mock->test;

	KUNIT_EXPECT_LE(test, timeout, 2000);
	KUNIT_EXPECT_GT(test, timeout, 0);
	KUNIT_EXPECT_LE(test, (u64)timeout, mock->deadline - mock->now);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 8), mock->sequence++);
	if (mock->step < 2) {
		KUNIT_EXPECT_EQ(test, length, 36);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data), 3);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 4), 0);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 12), 1 - mock->step);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 16),
				mock->step ? 0 : mock->queue);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(data + 20, 0, 16), NULL);
		mock->step++;
	} else {
		KUNIT_EXPECT_EQ(test, length, 128);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data), 0);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 4), 128);
		KUNIT_EXPECT_EQ(test, data[65], mock->queue);
	}
	mock->calls++;
	mock->now += 3;
	if (mock->calls != mock->fail_at)
		return length;
	switch (mock->fault) {
	case 0:
		return -EPIPE;
	case 1:
		return length - 1;
	case 2:
		return length + 1;
	case 3:
		mock->now = mock->deadline;
		break;
	case 4:
		mock->cancelled = true;
		break;
	}
	return length;
}

static struct ws73_usb_tx_io ws73_tx_io(struct ws73_tx_mock *mock)
{
	return (struct ws73_usb_tx_io) {
		.context = mock,
		.now_ms = ws73_tx_now,
		.cancelled = ws73_tx_cancelled,
		.send = ws73_tx_send,
	};
}

static const u8 ws73_tx_ble[] = { 0, 7, 4, 0, 1, 3, 0x0c, 0 };

static void ws73_usb_tx_sequence_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_usb_tx *other = kunit_kzalloc(test, sizeof(*other), GFP_KERNEL);
	struct ws73_tx_mock mock = { .test = test, .deadline = 100, .queue = 9 };
	struct ws73_usb_tx_io io = ws73_tx_io(&mock);
	struct ws73_tx_mock peer = mock;
	struct ws73_usb_tx_io peer_io = ws73_tx_io(&peer);
	u8 platform[32];

	KUNIT_ASSERT_NOT_NULL(test, tx);
	KUNIT_ASSERT_NOT_NULL(test, other);
	ws73_usb_tx_init(tx);
	ws73_usb_tx_init(other);
	KUNIT_ASSERT_EQ(test, btws73_chip_query_encode(platform, 32), 32);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, platform, 32, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	KUNIT_EXPECT_TRUE(test, tx->queue_valid);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, platform, 32, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 4);
	mock.queue = 7;
	mock.step = 0;
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 7);
	KUNIT_EXPECT_EQ(test, tx->queue, 7);
	/* A second instance still needs its own switch/reset and sequence zero. */
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(other, &peer_io, platform, 32, 100), 0);
	KUNIT_EXPECT_EQ(test, peer.calls, 3);
	KUNIT_EXPECT_EQ(test, tx->sequence, 7);
	/* USB counter wraps; no reply matcher depends on this value. */
	tx->sequence = U32_MAX;
	mock.sequence = U32_MAX;
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
	KUNIT_EXPECT_EQ(test, tx->sequence, 0);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
}

static void ws73_usb_tx_faults_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_tx_mock mock;
	struct ws73_usb_tx_io io = ws73_tx_io(&mock);
	unsigned int step, fault;
	int expected;

	KUNIT_ASSERT_NOT_NULL(test, tx);
	for (step = 1; step <= 3; step++) {
		for (fault = 0; fault < 5; fault++) {
			/* Fresh isolated synthetic session; no live callbacks exist. */
			ws73_usb_tx_init(tx);
			mock = (struct ws73_tx_mock) {
				.test = test, .deadline = 100, .queue = 7,
				.fail_at = step, .fault = fault,
			};
			expected = !fault ? -EPIPE : fault < 3 ? -EREMOTEIO :
				fault == 3 ? -ETIMEDOUT : -ENODEV;
			KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100),
					expected);
			KUNIT_EXPECT_EQ(test, mock.calls, step);
			KUNIT_EXPECT_FALSE(test, tx->queue_valid);
			/* Clearing external fault/deadline cannot resurrect this instance. */
			mock.cancelled = false;
			mock.now = 0;
			mock.fail_at = 0;
			KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100),
					expected);
			KUNIT_EXPECT_EQ(test, mock.calls, step);
		}
	}
}

static void ws73_usb_tx_admission_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_tx_mock mock = { .test = test, .deadline = 100, .queue = 7 };
	struct ws73_usb_tx_io io = ws73_tx_io(&mock);
	u8 invalid[8] = { 0, 8, 4 };

	KUNIT_ASSERT_NOT_NULL(test, tx);
	ws73_usb_tx_init(tx);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, invalid, 8, 100), -EOPNOTSUPP);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 3, 100), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, NULL, 8, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(NULL, &io, ws73_tx_ble, 8, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, NULL, ws73_tx_ble, 8, 100), -EINVAL);
	io.send = NULL;
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), -EINVAL);
	io.send = ws73_tx_send;
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	KUNIT_EXPECT_EQ(test, tx->failure, 0);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	/* Expired deadline/cancel before I/O still revoke the session. */
	ws73_usb_tx_init(tx);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, mock.now),
			-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	ws73_usb_tx_init(tx);
	mock.cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), -ENODEV);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
}

static struct kunit_case ws73_usb_tx_cases[] = {
	KUNIT_CASE(ws73_usb_tx_sequence_test),
	KUNIT_CASE(ws73_usb_tx_faults_test),
	KUNIT_CASE(ws73_usb_tx_admission_test),
	{}
};

static struct kunit_suite ws73_usb_tx_suite = {
	.name = "ws73-usb-tx",
	.test_cases = ws73_usb_tx_cases,
};

kunit_test_suite(ws73_usb_tx_suite);
