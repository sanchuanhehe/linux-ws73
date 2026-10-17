// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/unaligned.h>

#include "runtime.h"

struct ws73_runtime_mock {
	struct ws73_platform_request request;
	struct ws73_usb_tx tx;
	struct ws73_usb_tx_io io;
	struct completion sent;
	wait_queue_head_t stop_wait;
	u8 reply[32];
	u8 version_reply[32];
	u64 now;
	unsigned int calls;
	unsigned int mode;
	int received;
	int busy_result;
};

static u64 ws73_runtime_now(void *context)
{
	return ((struct ws73_runtime_mock *)context)->now;
}

static bool ws73_runtime_cancelled(void *context)
{
	return false;
}

static int ws73_runtime_send(void *context, const u8 *data, size_t length,
			     unsigned int timeout)
{
	struct ws73_runtime_mock *mock = context;

	mock->calls++;
	if (mock->mode == 11) {
		struct ws73_version version;

		mock->busy_result = ws73_platform_get_version(&mock->request, &mock->tx,
							     &mock->io, 100, &version);
	}
	if (mock->mode == 6) {
		ws73_platform_abort(&mock->request, -ENODEV);
		return length;
	}
	if (get_unaligned_le32(data))
		return length; /* Queue control precedes the HCC request. */
	if (get_unaligned_le16(data + 68) == 13) {
		if (data[72] != 1)
			return -EPROTO;
		if (mock->mode == 10)
			return length;
		if (mock->mode == 8) {
			mock->received = ws73_platform_receive(&mock->request, mock->reply,
							       32, mock->now);
			if (mock->received != -ENOENT)
				return -EPROTO;
		}
		ws73_platform_receive(&mock->request, mock->version_reply, 32, mock->now);
		if (mock->mode == 12)
			ws73_platform_receive(&mock->request, mock->version_reply, 32, mock->now);
		return length;
	}
	if (mock->mode == 13) {
		mock->received = ws73_platform_receive(&mock->request, mock->version_reply,
						       32, mock->now);
		if (mock->received != -ENOENT)
			return -EPROTO;
	}
	if (mock->mode == 1)
		return length; /* Real bounded timeout: no injected completion. */
	if (mock->mode == 7) {
		complete(&mock->sent);
		return length; /* A separate kernel thread supplies RX. */
	}
	if (mock->mode == 4)
		mock->now = mock->request.deadline;
	if (mock->mode == 5) {
		put_unaligned_le16(7, mock->reply + 4);
		mock->received = ws73_platform_receive(&mock->request, mock->reply, 32, mock->now);
		put_unaligned_le16(10, mock->reply + 4);
		if (mock->received != -ENOENT)
			return -EPROTO;
	}
	mock->received = ws73_platform_receive(&mock->request, mock->reply, 32, mock->now);
	if (mock->mode == 2)
		ws73_platform_receive(&mock->request, mock->reply, 32, mock->now);
	if (mock->mode == 3)
		return -EPIPE; /* A reply cannot conceal a local TX error. */
	return length;
}

static struct ws73_runtime_mock *ws73_runtime_fixture(struct kunit *test, unsigned int mode)
{
	struct ws73_runtime_mock *mock = kunit_kzalloc(test, sizeof(*mock), GFP_KERNEL);

	if (!mock)
		return NULL;
	ws73_platform_init(&mock->request);
	ws73_usb_tx_init(&mock->tx);
	init_completion(&mock->sent);
	init_waitqueue_head(&mock->stop_wait);
	mock->io = (struct ws73_usb_tx_io) {
		.context = mock,
		.now_ms = ws73_runtime_now,
		.cancelled = ws73_runtime_cancelled,
		.send = ws73_runtime_send,
	};
	mock->mode = mode;
	mock->reply[0] = 0x19;
	mock->reply[1] = 9;
	put_unaligned_le16(32, mock->reply + 2);
	put_unaligned_le16(10, mock->reply + 4);
	put_unaligned_le16(10, mock->reply + 6);
	put_unaligned_le32(2, mock->reply + 12);
	mock->reply[16] = 2;
	memcpy(mock->version_reply, mock->reply, 32);
	put_unaligned_le16(7, mock->version_reply + 4);
	put_unaligned_le16(16, mock->version_reply + 6);
	put_unaligned_le32(8, mock->version_reply + 12);
	put_unaligned_le32(0x010a6e7f, mock->version_reply + 16);
	put_unaligned_le32(0x12345678, mock->version_reply + 20);
	if (mode == 9)
		put_unaligned_le32(1, mock->version_reply + 8);
	return mock;
}

static void ws73_version_wire_test(struct kunit *test)
{
	static const u8 expected[32] = { 0x19, 9, 32, 0, 13, 0, 1, 0, 1 };
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 0);
	struct ws73_version version;
	u8 out[33];
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	memset(out, 0xa5, sizeof(out));
	for (i = 0; i < 32; i++) {
		KUNIT_EXPECT_EQ(test, ws73_version_query_encode(out, i), -ENOSPC);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(out, 0xa5, sizeof(out)), NULL);
	}
	KUNIT_ASSERT_EQ(test, ws73_version_query_encode(out, 32), 32);
	KUNIT_EXPECT_MEMEQ(test, out, expected, 32);
	KUNIT_EXPECT_EQ(test, out[32], 0xa5);
	/* Explicitly unaligned response, preserving even unknown version bits. */
	memcpy(out + 1, mock->version_reply, 32);
	KUNIT_ASSERT_EQ(test, ws73_version_reply_decode(out + 1, 32, &version), 0);
	KUNIT_EXPECT_EQ(test, version.firmware, 0x010a6e7fU);
	KUNIT_EXPECT_EQ(test, version.rom, 0x12345678U);
	memset(out + 17, 0, 8);
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(out + 1, 32, &version), 0);
	KUNIT_EXPECT_EQ(test, version.firmware, 0);
	KUNIT_EXPECT_EQ(test, version.rom, 0);
}

static void ws73_version_invalid_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 0);
	struct ws73_version version, before = { 0xa5a5a5a5, 0x5a5a5a5a };
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	for (i = 0; i < 32; i++) {
		version = before;
		KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, i, &version),
				-EMSGSIZE);
		KUNIT_EXPECT_MEMEQ(test, &version, &before, sizeof(version));
	}
	for (i = 0; i <= U16_MAX; i++) {
		version = before;
		put_unaligned_le16(i, mock->version_reply + 2);
		KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, 32, &version),
				i >= 24 && i <= 32 ? 0 : -EMSGSIZE);
		if (i < 24 || i > 32)
			KUNIT_EXPECT_MEMEQ(test, &version, &before, sizeof(version));
	}
	put_unaligned_le16(32, mock->version_reply + 2);
	version = before;
	put_unaligned_le16(10, mock->version_reply + 6);
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, 32, &version),
			-EMSGSIZE);
	put_unaligned_le16(16, mock->version_reply + 6);
	put_unaligned_le32(7, mock->version_reply + 12);
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, 32, &version),
			-EMSGSIZE);
	put_unaligned_le32(8, mock->version_reply + 12);
	put_unaligned_le32(1, mock->version_reply + 8);
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, 32, &version),
			-EREMOTEIO);
	put_unaligned_le32(0, mock->version_reply + 8);
	mock->version_reply[4] = 10;
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, 32, &version),
			-EPROTO);
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(NULL, 32, &version), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_version_reply_decode(mock->version_reply, 32, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_version_query_encode(NULL, 32), -EINVAL);
	KUNIT_EXPECT_MEMEQ(test, &version, &before, sizeof(version));
}

static void ws73_runtime_versions_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock;
	struct ws73_version version, before = { 0xa5a5a5a5, 0x5a5a5a5a };
	struct btws73_chip_identity chip;
	unsigned int mode;
	int expected;

	for (mode = 8; mode <= 13; mode++) {
		mock = ws73_runtime_fixture(test, mode);
		KUNIT_ASSERT_NOT_NULL(test, mock);
		KUNIT_ASSERT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
				&mock->io, 100, &chip), 0);
		version = before;
		expected = mode == 9 ? -EREMOTEIO : mode == 10 ? -ETIMEDOUT :
			mode == 12 ? -EPROTO : 0;
		KUNIT_EXPECT_EQ(test, ws73_platform_get_version(&mock->request, &mock->tx,
				&mock->io, mode == 10 ? 1 : 100, &version), expected);
		KUNIT_EXPECT_EQ(test, mock->calls, 4);
		if (!expected) {
			KUNIT_EXPECT_EQ(test, version.firmware, 0x010a6e7fU);
			KUNIT_EXPECT_EQ(test, version.rom, 0x12345678U);
			if (mode == 11)
				KUNIT_EXPECT_EQ(test, mock->busy_result, -EBUSY);
		} else {
			KUNIT_EXPECT_MEMEQ(test, &version, &before, sizeof(version));
		}
		KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
				&mock->io, 100, &chip), expected ? expected : -EALREADY);
		KUNIT_EXPECT_EQ(test, ws73_platform_get_version(&mock->request, &mock->tx,
				&mock->io, 100, &version), expected ? expected : -EALREADY);
		KUNIT_EXPECT_EQ(test, mock->calls, 4);
	}
}

static void ws73_runtime_query_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 0);
	struct btws73_chip_identity chip = { 0xa5, 0xa5 };

	KUNIT_ASSERT_NOT_NULL(test, mock);
	KUNIT_EXPECT_EQ(test, ws73_platform_receive(&mock->request, mock->reply, 32, 0),
			-EALREADY);
	KUNIT_ASSERT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
			&mock->io, 100, &chip), 0);
	KUNIT_EXPECT_EQ(test, chip.package, 2);
	KUNIT_EXPECT_EQ(test, chip.bus, 0);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
	KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
			&mock->io, 100, &chip), -EALREADY);
	KUNIT_EXPECT_EQ(test, ws73_platform_receive(&mock->request, mock->reply, 32, 0),
			-EALREADY);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
}

static void ws73_runtime_failures_test(struct kunit *test)
{
	static const int expected[] = { 0, -ETIMEDOUT, -EPROTO, -EPIPE,
				       -ETIMEDOUT, 0, -ENODEV };
	struct btws73_chip_identity chip, before = { 0xa5, 0xa5 };
	struct ws73_runtime_mock *mock;
	unsigned int mode, calls;

	for (mode = 1; mode <= 6; mode++) {
		mock = ws73_runtime_fixture(test, mode);
		KUNIT_ASSERT_NOT_NULL(test, mock);
		chip = before;
		KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
				&mock->io, mode == 1 ? 1 : 100, &chip), expected[mode]);
		calls = mock->calls;
		KUNIT_EXPECT_EQ(test, calls, mode == 6 ? 1 : 3);
		if (!expected[mode])
			continue;
		KUNIT_EXPECT_MEMEQ(test, &chip, &before, sizeof(chip));
		/* Even clearing the fake clock/fault cannot admit a late reply. */
		mock->now = 0;
		mock->mode = 0;
		KUNIT_EXPECT_EQ(test, ws73_platform_receive(&mock->request, mock->reply, 32, 0),
				expected[mode]);
		KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
				&mock->io, 100, &chip), expected[mode]);
		KUNIT_EXPECT_EQ(test, mock->calls, calls);
		KUNIT_EXPECT_MEMEQ(test, &chip, &before, sizeof(chip));
	}
}

static void ws73_runtime_reply_errors_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock;
	struct btws73_chip_identity chip, before = { 0xa5, 0xa5 };
	unsigned int i;
	int expected;

	for (i = 0; i < 4; i++) {
		mock = ws73_runtime_fixture(test, 0);
		KUNIT_ASSERT_NOT_NULL(test, mock);
		chip = before;
		if (i == 0) {
			put_unaligned_le32(1, mock->reply + 8);
			expected = -EREMOTEIO;
		} else if (i == 1) {
			mock->reply[16] = 3;
			expected = -EOPNOTSUPP;
		} else if (i == 2) {
			put_unaligned_le16(17, mock->reply + 2);
			expected = -EMSGSIZE;
		} else {
			put_unaligned_le32(3, mock->reply + 12);
			expected = -EMSGSIZE;
		}
		KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
				&mock->io, 100, &chip), expected);
		KUNIT_EXPECT_MEMEQ(test, &chip, &before, sizeof(chip));
	}
}

static void ws73_runtime_admission_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 0);
	struct btws73_chip_identity chip = { 0xa5, 0xa5 };

	KUNIT_ASSERT_NOT_NULL(test, mock);
	KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(NULL, &mock->tx, &mock->io, 100, &chip),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, NULL, &mock->io, 100, &chip),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx, NULL, 100, &chip),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx, &mock->io, 100, NULL),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_platform_receive(NULL, mock->reply, 32, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_platform_receive(&mock->request, NULL, 32, 0), -EINVAL);
	ws73_platform_abort(&mock->request, -ENODEV);
	ws73_platform_abort(&mock->request, -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx, &mock->io, 100, &chip),
			-ENODEV);
	KUNIT_EXPECT_EQ(test, mock->calls, 0);
	KUNIT_EXPECT_EQ(test, chip.package, 0xa5);
}

static int ws73_runtime_rx_thread(void *context)
{
	struct ws73_runtime_mock *mock = context;

	if (wait_for_completion_timeout(&mock->sent, HZ))
		mock->received = ws73_platform_receive(&mock->request, mock->reply, 32, 0);
	/* Keep the task alive until the test's managed teardown joins it. */
	wait_event_interruptible(mock->stop_wait, kthread_should_stop());
	return 0;
}

static void ws73_runtime_stop_thread(void *context)
{
	kthread_stop(context);
}

static void ws73_runtime_independent_rx_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 7);
	struct btws73_chip_identity chip;
	struct task_struct *task;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	task = kthread_run(ws73_runtime_rx_thread, mock, "ws73-test-rx");
	KUNIT_ASSERT_FALSE(test, IS_ERR(task));
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, ws73_runtime_stop_thread, task), 0);
	KUNIT_ASSERT_EQ(test, ws73_platform_get_chip(&mock->request, &mock->tx,
			&mock->io, 1000, &chip), 0);
	KUNIT_EXPECT_EQ(test, chip.package, 2);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
}

static struct kunit_case ws73_runtime_cases[] = {
	KUNIT_CASE(ws73_version_wire_test),
	KUNIT_CASE(ws73_version_invalid_test),
	KUNIT_CASE(ws73_runtime_versions_test),
	KUNIT_CASE(ws73_runtime_query_test),
	KUNIT_CASE(ws73_runtime_failures_test),
	KUNIT_CASE(ws73_runtime_reply_errors_test),
	KUNIT_CASE(ws73_runtime_admission_test),
	KUNIT_CASE(ws73_runtime_independent_rx_test),
	{}
};

static struct kunit_suite ws73_runtime_suite = {
	.name = "ws73-runtime",
	.test_cases = ws73_runtime_cases,
};

kunit_test_suite(ws73_runtime_suite);
