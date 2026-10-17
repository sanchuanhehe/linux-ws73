// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/unaligned.h>

#include "startup.h"

enum ws73_start_mock_mode {
	START_NORMAL,
	START_NO_CUSTOM,
	START_NO_ACTION,
	START_NO_BOOT,
	START_SHORT_CUSTOM,
	START_SHORT_CONTROL,
	START_ABORT_CUSTOM,
	START_ABORT_CONTROL,
	START_EXPIRE,
	START_REENTER,
	START_ASYNC,
	START_SHORT_PLATFORM,
};

struct ws73_start_mock {
	struct kunit *test;
	struct ws73_startup startup;
	struct ws73_usb_tx tx;
	struct ws73_usb_tx_io bulk;
	struct ws73_ble_control_io control;
	struct ws73_platform_config platform;
	struct ws73_customization parameters;
	struct completion waiting;
	struct completion enabled;
	wait_queue_head_t stopped;
	enum ws73_start_mock_mode mode;
	u64 now;
	unsigned int bulk_calls, control_calls;
	unsigned int control_timeout;
	int busy_result;
	bool cancelled;
};

static const struct ws73_interrupt ws73_start_bsp = {
	.events = WS73_INTERRUPT_BSP_READY,
	.high_priority_pool = U32_MAX, /* Not a device credit or handshake token. */
};

static int ws73_start_run(struct ws73_start_mock *mock)
{
	return ws73_startup_run(&mock->startup, &mock->tx, &mock->bulk,
				&mock->control, &mock->platform, &mock->parameters);
}

static u64 ws73_start_now(void *context)
{
	struct ws73_start_mock *mock = context;

	if (mock->mode == START_ASYNC && mock->startup.state == WS73_STARTUP_WAIT_BSP)
		complete(&mock->waiting);
	return mock->now;
}

static bool ws73_start_cancelled(void *context)
{
	return ((struct ws73_start_mock *)context)->cancelled;
}

static int ws73_start_bulk(void *context, const u8 *data, size_t length,
			   unsigned int timeout)
{
	struct ws73_start_mock *mock = context;
	struct kunit *test = mock->test;

	mock->bulk_calls++;
	KUNIT_EXPECT_TRUE(test, mock->startup.state == WS73_STARTUP_CUSTOMIZE ||
			  mock->startup.state == WS73_STARTUP_CONFIGURE);
	KUNIT_EXPECT_TRUE(test, mock->startup.bsp_seen);
	KUNIT_EXPECT_GT(test, timeout, 0);
	KUNIT_EXPECT_LE(test, timeout, 2000);
	if (length == 36)
		return length;
	if (mock->startup.state == WS73_STARTUP_CONFIGURE) {
		KUNIT_EXPECT_EQ(test, length, 128);
		KUNIT_EXPECT_EQ(test, data[64], 0x19);
		KUNIT_EXPECT_EQ(test, get_unaligned_le16(data + 68), 0);
		return mock->mode == START_SHORT_PLATFORM ? length - 1 : length;
	}
	KUNIT_EXPECT_EQ(test, length, 864);
	KUNIT_EXPECT_EQ(test, data[64], 0x50);
	KUNIT_EXPECT_EQ(test, data[65], 10);
	if (mock->mode != START_NO_CUSTOM)
		ws73_startup_receive(&mock->startup, WS73_NOTIFY_CUSTOMIZED, mock->now);
	if (mock->mode == START_ABORT_CUSTOM)
		ws73_startup_abort(&mock->startup, -ENODEV);
	if (mock->mode == START_REENTER)
		mock->busy_result = ws73_start_run(mock);
	if (mock->mode == START_EXPIRE)
		mock->now = mock->startup.deadline;
	return mock->mode == START_SHORT_CUSTOM ? length - 1 : length;
}

static int ws73_start_control(void *context, const u8 command[4], unsigned int timeout)
{
	struct ws73_start_mock *mock = context;
	struct kunit *test = mock->test;

	mock->control_calls++;
	mock->control_timeout = timeout;
	KUNIT_EXPECT_EQ(test, mock->startup.state, WS73_STARTUP_ENABLE);
	KUNIT_EXPECT_EQ(test, mock->startup.customization.state, WS73_CUSTOM_DONE);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(command), 25);
	if (mock->mode == START_ASYNC) {
		complete(&mock->enabled);
		return 4;
	}
	if (mock->mode != START_NO_ACTION)
		ws73_startup_receive(&mock->startup, WS73_NOTIFY_BLE_OPEN, mock->now);
	if (mock->mode != START_NO_BOOT)
		ws73_startup_receive(&mock->startup, WS73_NOTIFY_BOOT, mock->now);
	if (mock->mode == START_ABORT_CONTROL)
		ws73_startup_abort(&mock->startup, -ESHUTDOWN);
	return mock->mode == START_SHORT_CONTROL ? 3 : 4;
}

static void ws73_start_mock_init(struct ws73_start_mock *mock, struct kunit *test)
{
	memset(mock, 0, sizeof(*mock));
	mock->test = test;
	ws73_startup_init(&mock->startup);
	ws73_usb_tx_init(&mock->tx);
	init_completion(&mock->waiting);
	init_completion(&mock->enabled);
	init_waitqueue_head(&mock->stopped);
	mock->bulk = (struct ws73_usb_tx_io) {
		.context = mock, .now_ms = ws73_start_now,
		.cancelled = ws73_start_cancelled, .send = ws73_start_bulk,
	};
	mock->control = (struct ws73_ble_control_io) {
		.context = mock, .now_ms = ws73_start_now,
		.cancelled = ws73_start_cancelled, .send = ws73_start_control,
	};
}

static void ws73_startup_sequence_test(struct kunit *test)
{
	struct ws73_start_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	unsigned int early;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	for (early = 0; early < 2; early++) {
		ws73_start_mock_init(mock, test);
		KUNIT_EXPECT_EQ(test, ws73_startup_receive(&mock->startup, WS73_NOTIFY_BOOT, 0),
				-ENOENT);
		KUNIT_EXPECT_FALSE(test, mock->startup.boot_seen);
		KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, 0, 100), 0);
		if (early) {
			KUNIT_EXPECT_EQ(test, ws73_startup_receive(&mock->startup,
								   WS73_NOTIFY_BOOT, 0), 0);
			mock->mode = START_NO_BOOT; /* The earlier latch must survive. */
		}
		KUNIT_ASSERT_EQ(test,
				ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, 0), 0);
		KUNIT_ASSERT_EQ(test,
				ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, 0), 0);
		KUNIT_EXPECT_EQ(test, ws73_start_run(mock), 0);
		KUNIT_EXPECT_EQ(test, mock->startup.state, WS73_STARTUP_DONE);
		KUNIT_EXPECT_EQ(test, mock->bulk_calls, 6);
		KUNIT_EXPECT_EQ(test, mock->control_calls, 1);
		KUNIT_EXPECT_TRUE(test, mock->startup.boot_seen);
		KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -EALREADY);
		KUNIT_EXPECT_EQ(test, ws73_startup_arm(&mock->startup, 0, 100), -EALREADY);
		KUNIT_EXPECT_EQ(test, mock->control_calls, 1);
	}
}

static void ws73_startup_missing_test(struct kunit *test)
{
	struct ws73_start_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	unsigned int stage;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	for (stage = 0; stage < 4; stage++) {
		ws73_start_mock_init(mock, test);
		mock->mode = stage;
		KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, 0, 1), 0);
		/* An early BOOT cannot stand in for BSP, custom or action. */
		if (stage < 3)
			ws73_startup_receive(&mock->startup, WS73_NOTIFY_BOOT, 0);
		if (stage)
			ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, 0);
		KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -ETIMEDOUT);
		KUNIT_EXPECT_EQ(test, mock->startup.state, WS73_STARTUP_FAILED);
		KUNIT_EXPECT_EQ(test, mock->bulk_calls, stage ? 6 : 0);
		KUNIT_EXPECT_EQ(test, mock->control_calls, stage >= 2 ? 1 : 0);
		KUNIT_EXPECT_EQ(test, mock->startup.ble.state, WS73_BLE_FAILED);
		KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -ETIMEDOUT);
	}
}

static void ws73_startup_failures_test(struct kunit *test)
{
	static const struct {
		enum ws73_start_mock_mode mode;
		int error;
		unsigned int control_calls;
	} cases[] = {
		{ START_SHORT_CUSTOM, -EREMOTEIO, 0 }, { START_SHORT_CONTROL, -EMSGSIZE, 1 },
		{ START_ABORT_CUSTOM, -ENODEV, 0 }, { START_ABORT_CONTROL, -ESHUTDOWN, 1 },
		{ START_EXPIRE, -ETIMEDOUT, 0 },
		{ START_SHORT_PLATFORM, -EREMOTEIO, 0 },
	};
	struct ws73_start_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		ws73_start_mock_init(mock, test);
		mock->mode = cases[i].mode;
		KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, 0, 100), 0);
		ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, 0);
		KUNIT_EXPECT_EQ(test, ws73_start_run(mock), cases[i].error);
		KUNIT_EXPECT_EQ(test, mock->control_calls, cases[i].control_calls);
		ws73_startup_abort(&mock->startup, -EIO);
		KUNIT_EXPECT_EQ(test, ws73_start_run(mock), cases[i].error);
		KUNIT_EXPECT_EQ(test, mock->control_calls, cases[i].control_calls);
		KUNIT_EXPECT_EQ(test, mock->startup.customization.state, WS73_CUSTOM_FAILED);
		KUNIT_EXPECT_EQ(test, mock->startup.ble.state, WS73_BLE_FAILED);
	}
}

static void ws73_startup_fault_priority_test(struct kunit *test)
{
	struct ws73_startup startup;
	struct ws73_interrupt event;
	unsigned int i;
	int error;

	for (i = 0; i < 3; i++) {
		ws73_startup_init(&startup);
		KUNIT_ASSERT_EQ(test, ws73_startup_arm(&startup, 0, 100), 0);
		event = ws73_start_bsp;
		event.events |= i == 0 ? BIT(6) : i == 1 ? BIT(18) : BIT(5);
		error = i < 2 ? -EIO : -ENOBUFS;
		KUNIT_EXPECT_EQ(test, ws73_startup_interrupt(&startup, &event, 0), error);
		KUNIT_EXPECT_FALSE(test, startup.bsp_seen);
		KUNIT_EXPECT_EQ(test, ws73_startup_receive(&startup, WS73_NOTIFY_BOOT, 0), error);
		KUNIT_EXPECT_FALSE(test, startup.boot_seen);
	}
	ws73_startup_init(&startup);
	KUNIT_ASSERT_EQ(test, ws73_startup_arm(&startup, 0, 100), 0);
	event.events = BIT(4) | BIT(31);
	event.high_priority_pool = U32_MAX;
	KUNIT_EXPECT_EQ(test, ws73_startup_interrupt(&startup, &event, 0), -ENOENT);
	KUNIT_EXPECT_FALSE(test, startup.bsp_seen);
	KUNIT_EXPECT_EQ(test, ws73_startup_interrupt(&startup, &ws73_start_bsp, 100), -ETIMEDOUT);
	KUNIT_EXPECT_FALSE(test, startup.bsp_seen);
}

static void ws73_startup_admission_test(struct kunit *test)
{
	struct ws73_start_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, mock);
	ws73_start_mock_init(mock, test);
	KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -EAGAIN);
	KUNIT_EXPECT_EQ(test, mock->bulk_calls, 0);
	KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, 0, U64_MAX), 0);
	KUNIT_EXPECT_EQ(test, mock->startup.deadline, 12000);
	mock->parameters.calibration_count = 9;
	KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -ERANGE);
	KUNIT_EXPECT_EQ(test, mock->startup.state, WS73_STARTUP_ARMED);
	mock->parameters.calibration_count = 0;
	mock->platform.wake_gpio = 256;
	KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -ERANGE);
	KUNIT_EXPECT_EQ(test, mock->bulk_calls, 0);
	mock->platform.wake_gpio = 0;
	mock->mode = START_REENTER;
	ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, 0);
	KUNIT_ASSERT_EQ(test, ws73_start_run(mock), 0);
	KUNIT_EXPECT_EQ(test, mock->busy_result, -EBUSY);
	KUNIT_EXPECT_EQ(test, mock->control_timeout, 2000);
	ws73_start_mock_init(mock, test);
	mock->now = U64_MAX - 1;
	KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, mock->now, U64_MAX), 0);
	ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, mock->now);
	KUNIT_EXPECT_EQ(test, ws73_start_run(mock), 0);
	KUNIT_EXPECT_EQ(test, mock->control_timeout, 1);
	ws73_start_mock_init(mock, test);
	KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, 0, 100), 0);
	mock->cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_start_run(mock), -ENODEV);
	KUNIT_EXPECT_EQ(test, mock->bulk_calls, 0);
	ws73_start_mock_init(mock, test);
	KUNIT_EXPECT_EQ(test, ws73_startup_arm(&mock->startup, 100, 100), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_startup_arm(&mock->startup, 0, 100), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_startup_arm(NULL, 0, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_startup_run(NULL, &mock->tx, &mock->bulk,
					       &mock->control, &mock->platform,
					       &mock->parameters), -EINVAL);
}

static int ws73_start_rx_thread(void *context)
{
	struct ws73_start_mock *mock = context;

	if (wait_for_completion_timeout(&mock->waiting, msecs_to_jiffies(1000)))
		ws73_startup_interrupt(&mock->startup, &ws73_start_bsp, 0);
	if (wait_for_completion_timeout(&mock->enabled, msecs_to_jiffies(1000))) {
		ws73_startup_receive(&mock->startup, WS73_NOTIFY_BLE_OPEN, 0);
		ws73_startup_receive(&mock->startup, WS73_NOTIFY_BOOT, 0);
	}
	wait_event_interruptible(mock->stopped, kthread_should_stop());
	return 0;
}

static void ws73_startup_independent_rx_test(struct kunit *test)
{
	struct ws73_start_mock *mock = kunit_kmalloc(test, sizeof(*mock), GFP_KERNEL);
	struct task_struct *thread;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, mock);
	ws73_start_mock_init(mock, test);
	mock->mode = START_ASYNC;
	KUNIT_ASSERT_EQ(test, ws73_startup_arm(&mock->startup, 0, 1000), 0);
	thread = kthread_run(ws73_start_rx_thread, mock, "ws73-start-test");
	KUNIT_ASSERT_FALSE(test, IS_ERR(thread));
	ret = ws73_start_run(mock);
	kthread_stop(thread);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, mock->startup.state, WS73_STARTUP_DONE);
}

static struct kunit_case ws73_startup_cases[] = {
	KUNIT_CASE(ws73_startup_sequence_test),
	KUNIT_CASE(ws73_startup_missing_test),
	KUNIT_CASE(ws73_startup_failures_test),
	KUNIT_CASE(ws73_startup_fault_priority_test),
	KUNIT_CASE(ws73_startup_admission_test),
	KUNIT_CASE(ws73_startup_independent_rx_test),
	{}
};

static struct kunit_suite ws73_startup_suite = {
	.name = "ws73-startup",
	.test_cases = ws73_startup_cases,
};

kunit_test_suite(ws73_startup_suite);
