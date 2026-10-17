// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/kthread.h>

#include "customize-request.h"

enum ws73_custom_mock_mode {
	CUSTOM_REPLY,
	CUSTOM_MISSING,
	CUSTOM_DUPLICATE,
	CUSTOM_EARLY,
	CUSTOM_LATE,
	CUSTOM_ABORT,
	CUSTOM_CANCEL,
	CUSTOM_REENTER,
	CUSTOM_ASYNC,
	CUSTOM_EXPIRE_TX,
	CUSTOM_ABORT_BEFORE_TX,
};

struct ws73_custom_mock {
	struct ws73_customize_request request;
	struct ws73_usb_tx tx;
	struct ws73_usb_tx_io io;
	struct ws73_customization parameters;
	struct completion sent;
	wait_queue_head_t stopped;
	enum ws73_custom_mock_mode mode;
	u64 now;
	unsigned int calls, clocks, timeout, fail_at;
	int fault, receive_result, busy_result;
	bool cancelled;
};

static u64 ws73_custom_mock_now(void *context)
{
	struct ws73_custom_mock *mock = context;

	if (++mock->clocks == 2 && mock->mode == CUSTOM_ABORT_BEFORE_TX)
		ws73_customize_abort(&mock->request, -ECANCELED);
	return mock->now;
}

static bool ws73_custom_mock_cancelled(void *context)
{
	return ((struct ws73_custom_mock *)context)->cancelled;
}

static int ws73_custom_mock_send(void *context, const u8 *data, size_t length,
				 unsigned int timeout)
{
	struct ws73_custom_mock *mock = context;

	mock->calls++;
	mock->timeout = timeout;
	if (mock->mode == CUSTOM_EARLY || length == 864) {
		if (mock->mode == CUSTOM_MISSING) {
			ws73_customize_receive(&mock->request, WS73_NOTIFY_BOOT, mock->now);
			ws73_customize_receive(&mock->request, WS73_NOTIFY_BLE_OPEN, mock->now);
			ws73_customize_receive(&mock->request, WS73_NOTIFY_BLE_CLOSE, mock->now);
		} else if (mock->mode == CUSTOM_ASYNC) {
			complete(&mock->sent);
		} else {
			if (mock->mode == CUSTOM_LATE)
				mock->now = mock->request.deadline;
			mock->receive_result =
				ws73_customize_receive(&mock->request,
						       WS73_NOTIFY_CUSTOMIZED, mock->now);
			if (mock->mode == CUSTOM_DUPLICATE)
				ws73_customize_receive(&mock->request,
						       WS73_NOTIFY_CUSTOMIZED, mock->now);
		}
		if (mock->mode == CUSTOM_ABORT) {
			ws73_customize_abort(&mock->request, -ESHUTDOWN);
			ws73_customize_abort(&mock->request, -EIO);
		}
		if (mock->mode == CUSTOM_CANCEL)
			mock->cancelled = true;
		if (mock->mode == CUSTOM_REENTER)
			mock->busy_result = ws73_customize_apply(&mock->request, &mock->tx,
								 &mock->io, &mock->parameters, 100);
		if (mock->mode == CUSTOM_EXPIRE_TX)
			mock->now = mock->request.deadline;
	}
	if (mock->calls == mock->fail_at)
		return mock->fault < 0 ? mock->fault : length + mock->fault - 1;
	return length;
}

static void ws73_custom_mock_init(struct ws73_custom_mock *mock)
{
	memset(mock, 0, sizeof(*mock));
	ws73_customize_request_init(&mock->request);
	ws73_usb_tx_init(&mock->tx);
	init_completion(&mock->sent);
	init_waitqueue_head(&mock->stopped);
	mock->io = (struct ws73_usb_tx_io) {
		.context = mock, .now_ms = ws73_custom_mock_now,
		.cancelled = ws73_custom_mock_cancelled, .send = ws73_custom_mock_send,
	};
}

static int ws73_custom_apply(struct ws73_custom_mock *mock, u64 deadline)
{
	return ws73_customize_apply(&mock->request, &mock->tx, &mock->io,
				    &mock->parameters, deadline);
}

static void ws73_custom_request_lifecycle_test(struct kunit *test)
{
	struct ws73_custom_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	struct ws73_customize_request other;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	ws73_custom_mock_init(mock);
	ws73_customize_request_init(&other);
	KUNIT_EXPECT_EQ(test, ws73_customize_receive(&mock->request,
						     WS73_NOTIFY_CUSTOMIZED, 0), -ENOENT);
	KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_UNUSED);
	KUNIT_ASSERT_EQ(test, ws73_custom_apply(mock, 100), 0);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
	KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_DONE);
	KUNIT_EXPECT_EQ(test, other.state, WS73_CUSTOM_UNUSED);
	KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), -EALREADY);
	KUNIT_EXPECT_EQ(test, mock->calls, 3);
	KUNIT_EXPECT_EQ(test, ws73_customize_receive(&mock->request,
						     WS73_NOTIFY_CUSTOMIZED, 0), -EPROTO);
	KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_FAILED);
	KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), -EPROTO);
}

static void ws73_custom_request_tx_faults_test(struct kunit *test)
{
	static const int faults[] = { -EPIPE, 0, 2 };
	struct ws73_custom_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	unsigned int step, i;
	int expected;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	for (step = 1; step <= 3; step++) {
		for (i = 0; i < ARRAY_SIZE(faults); i++) {
			ws73_custom_mock_init(mock);
			mock->fail_at = step;
			mock->fault = faults[i];
			expected = i ? -EREMOTEIO : -EPIPE;
			KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), expected);
			KUNIT_EXPECT_EQ(test, mock->calls, step);
			KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_FAILED);
			mock->fail_at = 0;
			KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), expected);
			KUNIT_EXPECT_EQ(test, mock->calls, step);
		}
	}
}

static void ws73_custom_request_reply_faults_test(struct kunit *test)
{
	static const struct {
		enum ws73_custom_mock_mode mode;
		int error;
	} cases[] = {
		{ CUSTOM_MISSING, -ETIMEDOUT }, { CUSTOM_DUPLICATE, -EPROTO },
		{ CUSTOM_EARLY, -EPROTO }, { CUSTOM_LATE, -ETIMEDOUT },
		{ CUSTOM_ABORT, -ESHUTDOWN }, { CUSTOM_CANCEL, -ENODEV },
		{ CUSTOM_EXPIRE_TX, -ETIMEDOUT }, { CUSTOM_ABORT_BEFORE_TX, -ECANCELED },
	};
	struct ws73_custom_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	unsigned int i, calls;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		ws73_custom_mock_init(mock);
		mock->mode = cases[i].mode;
		KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 1), cases[i].error);
		calls = mock->mode == CUSTOM_EARLY ? 1 :
			mock->mode == CUSTOM_ABORT_BEFORE_TX ? 0 : 3;
		KUNIT_EXPECT_EQ(test, mock->calls, calls);
		KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_FAILED);
		mock->mode = CUSTOM_REPLY;
		mock->now = 0;
		mock->cancelled = false;
		KUNIT_EXPECT_EQ(test, ws73_customize_receive(&mock->request,
							     WS73_NOTIFY_CUSTOMIZED, 0),
				cases[i].error);
		KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), cases[i].error);
		KUNIT_EXPECT_EQ(test, mock->calls, calls);
	}
}

static void ws73_custom_request_admission_test(struct kunit *test)
{
	struct ws73_custom_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, mock);
	ws73_custom_mock_init(mock);
	mock->parameters.calibration_count = 9;
	KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), -ERANGE);
	KUNIT_EXPECT_EQ(test, mock->calls, 0);
	KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_UNUSED);
	mock->parameters.calibration_count = 0;
	mock->mode = CUSTOM_REENTER;
	KUNIT_ASSERT_EQ(test, ws73_custom_apply(mock, U64_MAX), 0);
	KUNIT_EXPECT_EQ(test, mock->busy_result, -EBUSY);
	KUNIT_EXPECT_EQ(test, mock->timeout, 2000);
	ws73_custom_mock_init(mock);
	mock->now = U64_MAX - 1;
	KUNIT_ASSERT_EQ(test, ws73_custom_apply(mock, U64_MAX), 0);
	KUNIT_EXPECT_EQ(test, mock->timeout, 1);
	ws73_custom_mock_init(mock);
	mock->cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), -ENODEV);
	KUNIT_EXPECT_EQ(test, mock->calls, 0);
	ws73_custom_mock_init(mock);
	KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 0), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock->calls, 0);
	KUNIT_EXPECT_EQ(test, ws73_customize_apply(NULL, &mock->tx,
						   &mock->io, &mock->parameters, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_customize_apply(&mock->request, NULL,
						   &mock->io, &mock->parameters, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_customize_apply(&mock->request, &mock->tx,
						   NULL, &mock->parameters, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_customize_apply(&mock->request, &mock->tx,
						   &mock->io, NULL, 100), -EINVAL);
	mock->io.send = NULL;
	KUNIT_EXPECT_EQ(test, ws73_custom_apply(mock, 100), -EINVAL);
}

static int ws73_custom_rx_thread(void *context)
{
	struct ws73_custom_mock *mock = context;

	if (wait_for_completion_timeout(&mock->sent, msecs_to_jiffies(1000)))
		mock->receive_result = ws73_customize_receive(&mock->request,
							      WS73_NOTIFY_CUSTOMIZED, 0);
	wait_event_interruptible(mock->stopped, kthread_should_stop());
	return 0;
}

static void ws73_custom_request_independent_rx_test(struct kunit *test)
{
	struct ws73_custom_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	struct task_struct *thread;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	ws73_custom_mock_init(mock);
	mock->mode = CUSTOM_ASYNC;
	mock->receive_result = -EINPROGRESS;
	thread = kthread_run(ws73_custom_rx_thread, mock, "ws73-custom-test");
	KUNIT_ASSERT_FALSE(test, IS_ERR(thread));
	ret = ws73_custom_apply(mock, 1000);
	kthread_stop(thread);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, mock->receive_result, 0);
	KUNIT_EXPECT_EQ(test, mock->request.state, WS73_CUSTOM_DONE);
}

static struct kunit_case ws73_custom_request_cases[] = {
	KUNIT_CASE(ws73_custom_request_lifecycle_test),
	KUNIT_CASE(ws73_custom_request_tx_faults_test),
	KUNIT_CASE(ws73_custom_request_reply_faults_test),
	KUNIT_CASE(ws73_custom_request_admission_test),
	KUNIT_CASE(ws73_custom_request_independent_rx_test),
	{}
};

static struct kunit_suite ws73_custom_request_suite = {
	.name = "ws73-custom-request",
	.test_cases = ws73_custom_request_cases,
};

kunit_test_suite(ws73_custom_request_suite);
