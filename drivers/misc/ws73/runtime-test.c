// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/unaligned.h>

#include "runtime.h"

struct ws73_runtime_mock {
	struct ws73_identity_request request;
	struct ws73_usb_tx tx;
	struct ws73_usb_tx_io io;
	struct completion sent;
	wait_queue_head_t stop_wait;
	u8 reply[32];
	u64 now;
	unsigned int calls;
	unsigned int mode;
	int received;
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
	if (mock->mode == 6) {
		ws73_identity_abort(&mock->request, -ENODEV);
		return length;
	}
	if (get_unaligned_le32(data))
		return length; /* Queue control precedes the HCC request. */
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
		mock->received = ws73_identity_receive(&mock->request, mock->reply, 32, mock->now);
		put_unaligned_le16(10, mock->reply + 4);
		if (mock->received != -ENOENT)
			return -EPROTO;
	}
	mock->received = ws73_identity_receive(&mock->request, mock->reply, 32, mock->now);
	if (mock->mode == 2)
		ws73_identity_receive(&mock->request, mock->reply, 32, mock->now);
	if (mock->mode == 3)
		return -EPIPE; /* A reply cannot conceal a local TX error. */
	return length;
}

static struct ws73_runtime_mock *ws73_runtime_fixture(struct kunit *test, unsigned int mode)
{
	struct ws73_runtime_mock *mock = kunit_kzalloc(test, sizeof(*mock), GFP_KERNEL);

	if (!mock)
		return NULL;
	ws73_identity_init(&mock->request);
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
	return mock;
}

static void ws73_runtime_query_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 0);
	struct btws73_chip_identity chip = { 0xa5, 0xa5 };

	KUNIT_ASSERT_NOT_NULL(test, mock);
	KUNIT_EXPECT_EQ(test, ws73_identity_receive(&mock->request, mock->reply, 32, 0),
			-EALREADY);
	KUNIT_ASSERT_EQ(test, ws73_identity_query(&mock->request, &mock->tx,
			&mock->io, 100, &chip), 0);
	KUNIT_EXPECT_EQ(test, chip.package, 2);
	KUNIT_EXPECT_EQ(test, chip.bus, 0);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
	KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx,
			&mock->io, 100, &chip), -EALREADY);
	KUNIT_EXPECT_EQ(test, ws73_identity_receive(&mock->request, mock->reply, 32, 0),
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
		KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx,
				&mock->io, mode == 1 ? 1 : 100, &chip), expected[mode]);
		calls = mock->calls;
		KUNIT_EXPECT_EQ(test, calls, mode == 6 ? 1 : 3);
		if (!expected[mode])
			continue;
		KUNIT_EXPECT_MEMEQ(test, &chip, &before, sizeof(chip));
		/* Even clearing the fake clock/fault cannot admit a late reply. */
		mock->now = 0;
		mock->mode = 0;
		KUNIT_EXPECT_EQ(test, ws73_identity_receive(&mock->request, mock->reply, 32, 0),
				expected[mode]);
		KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx,
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
		KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx,
				&mock->io, 100, &chip), expected);
		KUNIT_EXPECT_MEMEQ(test, &chip, &before, sizeof(chip));
	}
}

static void ws73_runtime_admission_test(struct kunit *test)
{
	struct ws73_runtime_mock *mock = ws73_runtime_fixture(test, 0);
	struct btws73_chip_identity chip = { 0xa5, 0xa5 };

	KUNIT_ASSERT_NOT_NULL(test, mock);
	KUNIT_EXPECT_EQ(test, ws73_identity_query(NULL, &mock->tx, &mock->io, 100, &chip),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, NULL, &mock->io, 100, &chip),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx, NULL, 100, &chip),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx, &mock->io, 100, NULL),
			-EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_identity_receive(NULL, mock->reply, 32, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_identity_receive(&mock->request, NULL, 32, 0), -EINVAL);
	ws73_identity_abort(&mock->request, -ENODEV);
	ws73_identity_abort(&mock->request, -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_identity_query(&mock->request, &mock->tx, &mock->io, 100, &chip),
			-ENODEV);
	KUNIT_EXPECT_EQ(test, mock->calls, 0);
	KUNIT_EXPECT_EQ(test, chip.package, 0xa5);
}

static int ws73_runtime_rx_thread(void *context)
{
	struct ws73_runtime_mock *mock = context;

	if (wait_for_completion_timeout(&mock->sent, HZ))
		mock->received = ws73_identity_receive(&mock->request, mock->reply, 32, 0);
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
	KUNIT_ASSERT_EQ(test, ws73_identity_query(&mock->request, &mock->tx,
			&mock->io, 1000, &chip), 0);
	KUNIT_EXPECT_EQ(test, chip.package, 2);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
}

static struct kunit_case ws73_runtime_cases[] = {
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
