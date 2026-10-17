// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/unaligned.h>

#include "ble-control.h"

struct ws73_ble_mock {
	struct ws73_ble_control control;
	struct ws73_ble_control_io io;
	struct completion sent;
	wait_queue_head_t stopped;
	u64 now;
	unsigned int calls, clocks, mode, timeout;
	int tx_result, busy_result, receive_result;
	bool cancelled;
	u8 command[4];
};

static u64 ws73_ble_now(void *context)
{
	struct ws73_ble_mock *mock = context;

	if (++mock->clocks == 2 && mock->mode == 8)
		ws73_ble_control_abort(&mock->control, -ECANCELED);
	return mock->now;
}

static bool ws73_ble_cancelled(void *context)
{
	return ((struct ws73_ble_mock *)context)->cancelled;
}

static int ws73_ble_send(void *context, const u8 command[4], unsigned int timeout)
{
	struct ws73_ble_mock *mock = context;
	enum ws73_notification event = command[0] == 25 ?
		WS73_NOTIFY_BLE_OPEN : WS73_NOTIFY_BLE_CLOSE;

	mock->calls++;
	mock->timeout = timeout;
	memcpy(mock->command, command, 4);
	if (mock->mode == 1) {
		ws73_ble_control_receive(&mock->control, WS73_NOTIFY_BOOT, mock->now);
		ws73_ble_control_receive(&mock->control, WS73_NOTIFY_CUSTOMIZED, mock->now);
		return 4; /* No action reply: real bounded wait must time out. */
	}
	if (mock->mode == 7) {
		complete(&mock->sent);
		return 4; /* Independent RX thread. */
	}
	if (mock->mode == 3)
		event = event == WS73_NOTIFY_BLE_OPEN ?
			WS73_NOTIFY_BLE_CLOSE : WS73_NOTIFY_BLE_OPEN;
	if (mock->mode == 4)
		mock->now = mock->control.deadline;
	if (mock->mode == 6)
		mock->busy_result = ws73_ble_control_set(&mock->control, &mock->io, true, 100);
	mock->receive_result = ws73_ble_control_receive(&mock->control, event, mock->now);
	if (mock->mode == 2)
		ws73_ble_control_receive(&mock->control, event, mock->now);
	if (mock->mode == 5) {
		ws73_ble_control_abort(&mock->control, -ENODEV);
		ws73_ble_control_abort(&mock->control, -EIO);
	}
	if (mock->mode == 9)
		mock->cancelled = true;
	if (mock->mode == 10)
		mock->now = mock->control.deadline; /* Reply was timely; TX was not. */
	return mock->tx_result;
}

static void ws73_ble_mock_init(struct ws73_ble_mock *mock)
{
	memset(mock, 0, sizeof(*mock));
	ws73_ble_control_init(&mock->control);
	init_completion(&mock->sent);
	init_waitqueue_head(&mock->stopped);
	mock->tx_result = 4;
	mock->io = (struct ws73_ble_control_io) {
		.context = mock,
		.now_ms = ws73_ble_now,
		.cancelled = ws73_ble_cancelled,
		.send = ws73_ble_send,
	};
}

static void ws73_ble_lifecycle_test(struct kunit *test)
{
	struct ws73_ble_mock mock, other;
	unsigned int i;

	ws73_ble_mock_init(&mock);
	ws73_ble_mock_init(&other);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_receive(&mock.control, WS73_NOTIFY_BLE_OPEN, 0),
			-ENOENT);
	KUNIT_EXPECT_EQ(test, mock.control.state, WS73_BLE_UNKNOWN);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, false, 100), -EAGAIN);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	for (i = 0; i < 100; i++) {
		bool enabled = !(i & 1);
		u8 expected[] = { enabled ? 25 : 26, 0, 0, 0 };

		KUNIT_ASSERT_EQ(test,
			ws73_ble_control_set(&mock.control, &mock.io, enabled, 100), 0);
		KUNIT_EXPECT_MEMEQ(test, mock.command, expected, 4);
		KUNIT_EXPECT_EQ(test, mock.calls, i + 1);
		KUNIT_EXPECT_EQ(test, mock.timeout, 100);
		KUNIT_EXPECT_EQ(test, mock.control.state,
				enabled ? WS73_BLE_ENABLED : WS73_BLE_DISABLED);
		KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, enabled, 100),
				-EALREADY);
		KUNIT_EXPECT_EQ(test, mock.calls, i + 1);
		KUNIT_EXPECT_EQ(test, other.control.state, WS73_BLE_UNKNOWN);
	}
	/* An unsolicited duplicate after consumption is ambiguous and terminal. */
	KUNIT_EXPECT_EQ(test, ws73_ble_control_receive(&mock.control, WS73_NOTIFY_BLE_CLOSE, 0),
			-EPROTO);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100), -EPROTO);
	KUNIT_EXPECT_EQ(test, mock.calls, 100);
}

static void ws73_ble_tx_failure_test(struct kunit *test)
{
	static const int results[] = { -EPIPE, 0, 1, 2, 3, 5, 4096 };
	struct ws73_ble_mock mock;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(results); i++) {
		ws73_ble_mock_init(&mock);
		mock.tx_result = results[i];
		KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100),
				results[i] < 0 ? results[i] : -EMSGSIZE);
		KUNIT_EXPECT_EQ(test, mock.receive_result, 0); /* Reply cannot hide failed TX. */
		KUNIT_EXPECT_EQ(test, mock.control.state, WS73_BLE_FAILED);
		KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, false, 100),
				results[i] < 0 ? results[i] : -EMSGSIZE);
		KUNIT_EXPECT_EQ(test, mock.calls, 1);
	}
}

static void ws73_ble_reply_failure_test(struct kunit *test)
{
	static const unsigned int modes[] = { 1, 2, 3, 4, 5, 9, 10 };
	static const int errors[] = {
		-ETIMEDOUT, -EPROTO, -EPROTO, -ETIMEDOUT, -ENODEV, -ENODEV, -ETIMEDOUT,
	};
	struct ws73_ble_mock mock;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(modes); i++) {
		ws73_ble_mock_init(&mock);
		mock.mode = modes[i];
		KUNIT_EXPECT_EQ(test,
			ws73_ble_control_set(&mock.control, &mock.io, true, 1), errors[i]);
		KUNIT_EXPECT_EQ(test, mock.control.state, WS73_BLE_FAILED);
		KUNIT_EXPECT_EQ(test,
			ws73_ble_control_receive(&mock.control, WS73_NOTIFY_BLE_OPEN, 0),
			errors[i]);
		KUNIT_EXPECT_EQ(test,
			ws73_ble_control_set(&mock.control, &mock.io, true, 100), errors[i]);
		KUNIT_EXPECT_EQ(test, mock.calls, 1);
	}
	/* Failed close must not publish DISABLED or permit a reopen. */
	ws73_ble_mock_init(&mock);
	KUNIT_ASSERT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100), 0);
	mock.mode = 1;
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, false, 1), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.control.state, WS73_BLE_FAILED);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.calls, 2);
}

static void ws73_ble_admission_test(struct kunit *test)
{
	struct ws73_ble_mock mock;

	ws73_ble_mock_init(&mock);
	mock.mode = 6;
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, U64_MAX), 0);
	KUNIT_EXPECT_EQ(test, mock.busy_result, -EBUSY);
	KUNIT_EXPECT_EQ(test, mock.timeout, 2000);
	KUNIT_EXPECT_EQ(test, mock.calls, 1);
	ws73_ble_mock_init(&mock);
	mock.now = U64_MAX - 1;
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, U64_MAX), 0);
	KUNIT_EXPECT_EQ(test, mock.timeout, 1);
	ws73_ble_mock_init(&mock);
	mock.cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100), -ENODEV);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	ws73_ble_mock_init(&mock);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 0), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	ws73_ble_mock_init(&mock);
	mock.mode = 8;
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100), -ECANCELED);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(NULL, &mock.io, true, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, NULL, true, 100), -EINVAL);
	mock.io.send = NULL;
	KUNIT_EXPECT_EQ(test, ws73_ble_control_set(&mock.control, &mock.io, true, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_ble_control_receive(NULL, WS73_NOTIFY_BLE_OPEN, 0), -EINVAL);
}

static int ws73_ble_rx_thread(void *context)
{
	struct ws73_ble_mock *mock = context;

	if (wait_for_completion_timeout(&mock->sent, msecs_to_jiffies(1000)))
		mock->receive_result = ws73_ble_control_receive(&mock->control,
							       WS73_NOTIFY_BLE_OPEN, 0);
	wait_event_interruptible(mock->stopped, kthread_should_stop());
	return 0;
}

static void ws73_ble_independent_rx_test(struct kunit *test)
{
	struct ws73_ble_mock mock;
	struct task_struct *thread;
	int ret;

	ws73_ble_mock_init(&mock);
	mock.mode = 7;
	mock.receive_result = -EINPROGRESS;
	thread = kthread_run(ws73_ble_rx_thread, &mock, "ws73-ble-test");
	KUNIT_ASSERT_FALSE(test, IS_ERR(thread));
	ret = ws73_ble_control_set(&mock.control, &mock.io, true, 1000);
	kthread_stop(thread);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, mock.receive_result, 0);
	KUNIT_EXPECT_EQ(test, mock.control.state, WS73_BLE_ENABLED);
}

static struct kunit_case ws73_ble_control_cases[] = {
	KUNIT_CASE(ws73_ble_lifecycle_test),
	KUNIT_CASE(ws73_ble_tx_failure_test),
	KUNIT_CASE(ws73_ble_reply_failure_test),
	KUNIT_CASE(ws73_ble_admission_test),
	KUNIT_CASE(ws73_ble_independent_rx_test),
	{}
};

static struct kunit_suite ws73_ble_control_suite = {
	.name = "ws73-ble-control",
	.test_cases = ws73_ble_control_cases,
};
kunit_test_suite(ws73_ble_control_suite);

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
