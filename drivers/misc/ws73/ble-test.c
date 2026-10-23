// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>

#include "ble.h"

struct ws73_ble_fixture {
	struct device *parent;
	struct auxiliary_device *adev;
	unsigned int starts, stops, sends, receives, faults;
	int action_error, send_error, fault;
	u8 received;
	bool block_start, block_send;
	struct completion entered, release, faulted, operation_done, removed;
	struct task_struct *operator, *remover;
	wait_queue_head_t stop;
	struct device *held;
	int operation_result;
};

static int ws73_ble_test_block(struct ws73_ble_fixture *f)
{
	complete(&f->entered);
	return wait_for_completion_timeout(&f->release, 5 * HZ) ? 0 : -ETIMEDOUT;
}

static int ws73_ble_test_set(void *context, bool enabled)
{
	struct ws73_ble_fixture *f = context;

	if (enabled)
		f->starts++;
	else
		f->stops++;
	if (enabled && f->block_start)
		return ws73_ble_test_block(f);
	return f->action_error;
}

static int ws73_ble_test_send(void *context, const u8 *data, size_t length)
{
	struct ws73_ble_fixture *f = context;

	f->sends++;
	if (f->block_send)
		return ws73_ble_test_block(f);
	return f->send_error;
}

static void ws73_ble_test_receive(void *context, const u8 *data, size_t length)
{
	struct ws73_ble_fixture *f = context;

	f->receives++;
	if (length)
		f->received = data[0];
}

static void ws73_ble_test_fault(void *context, int error)
{
	struct ws73_ble_fixture *f = context;

	f->faults++;
	f->fault = error;
	complete(&f->faulted);
}

static const struct ws73_ble_backend ws73_ble_test_backend = {
	.set_enabled = ws73_ble_test_set,
	.send = ws73_ble_test_send,
};

static const struct ws73_ble_client ws73_ble_test_client = {
	.receive = ws73_ble_test_receive,
	.fault = ws73_ble_test_fault,
};

static const u8 ws73_ble_test_record[] = { 0, 7, 4, 0, 1, 3, 12, 0 };

static int ws73_ble_test_init(struct kunit *test)
{
	struct ws73_ble_fixture *f;

	f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	test->priv = f;
	init_completion(&f->entered);
	init_completion(&f->release);
	init_completion(&f->faulted);
	init_completion(&f->operation_done);
	init_completion(&f->removed);
	init_waitqueue_head(&f->stop);
	f->parent = root_device_register(test->name);
	if (IS_ERR(f->parent))
		return PTR_ERR(f->parent);
	f->adev = ws73_ble_add(f->parent, &ws73_ble_test_backend, f);
	if (IS_ERR(f->adev)) {
		root_device_unregister(f->parent);
		return PTR_ERR(f->adev);
	}
	return 0;
}

static void ws73_ble_test_exit(struct kunit *test)
{
	struct ws73_ble_fixture *f = test->priv;

	/* Assertion failures must also release and join both producers. */
	complete_all(&f->release);
	if (f->operator) {
		kthread_stop(f->operator);
		put_task_struct(f->operator);
	}
	if (f->remover) {
		kthread_stop(f->remover);
		put_task_struct(f->remover);
		f->adev = NULL;
	}
	if (f->adev)
		ws73_ble_remove(f->adev);
	if (f->held)
		put_device(f->held);
	root_device_unregister(f->parent);
}

static void ws73_ble_lifecycle_test(struct kunit *test)
{
	struct ws73_ble_fixture *f = test->priv;
	u8 event = 4;

	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->adev), -ENOTCONN);
	KUNIT_EXPECT_EQ(test, f->starts, 0);
	KUNIT_ASSERT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), -EBUSY);
	ws73_ble_receive(f->adev, &event, 1);
	KUNIT_EXPECT_EQ(test, f->receives, 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, ws73_ble_test_record, 8), -ENETDOWN);
	KUNIT_ASSERT_EQ(test, ws73_ble_start(f->adev), 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->adev), -EALREADY);
	KUNIT_EXPECT_EQ(test, f->starts, 1);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, ws73_ble_test_record, 8), 0);
	KUNIT_EXPECT_EQ(test, f->sends, 1);
	ws73_ble_receive(f->adev, &event, 1);
	KUNIT_EXPECT_EQ(test, f->receives, 1);
	KUNIT_EXPECT_EQ(test, f->received, event);
	KUNIT_ASSERT_EQ(test, ws73_ble_stop(f->adev), 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_stop(f->adev), -EALREADY);
	KUNIT_EXPECT_EQ(test, f->stops, 1);
	ws73_ble_receive(f->adev, &event, 1);
	KUNIT_EXPECT_EQ(test, f->receives, 1);
	ws73_ble_unbind(f->adev);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->adev), -ENOTCONN);
	KUNIT_ASSERT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), 0);
	KUNIT_ASSERT_EQ(test, ws73_ble_start(f->adev), 0);
	KUNIT_EXPECT_EQ(test, f->starts, 2);
	ws73_ble_unbind(f->adev);
	KUNIT_EXPECT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), -EBUSY);
	KUNIT_ASSERT_EQ(test, ws73_ble_stop(f->adev), 0);
}

static void ws73_ble_failure_release_test(struct kunit *test)
{
	struct ws73_ble_fixture *f = test->priv;
	struct auxiliary_device *held = f->adev;
	u8 event = 4;

	KUNIT_ASSERT_EQ(test, ws73_ble_bind(held, &ws73_ble_test_client, f), 0);
	f->action_error = -ETIMEDOUT;
	KUNIT_EXPECT_EQ(test, ws73_ble_start(held), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, f->faults, 1);
	KUNIT_EXPECT_EQ(test, f->fault, -ETIMEDOUT);
	f->action_error = 0;
	KUNIT_EXPECT_EQ(test, ws73_ble_start(held), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, f->starts, 1);
	ws73_ble_receive(held, &event, 1);
	KUNIT_EXPECT_EQ(test, f->receives, 0);
	get_device(&held->dev);
	ws73_ble_remove(held);
	f->adev = NULL;
	KUNIT_EXPECT_EQ(test, f->faults, 1);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(held, ws73_ble_test_record, 8), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_ble_bind(held, &ws73_ble_test_client, f), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_ble_stop(held), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, f->sends, 0);
	KUNIT_EXPECT_EQ(test, f->stops, 0);
	put_device(&held->dev);
}

static void ws73_ble_send_failure_test(struct kunit *test)
{
	struct ws73_ble_fixture *f = test->priv;
	u8 invalid[16] = { 0, 7, 4, 0, 1, 3, 12, 0 };

	KUNIT_ASSERT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), 0);
	KUNIT_ASSERT_EQ(test, ws73_ble_start(f->adev), 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, NULL, 8), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, invalid, 7), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, invalid, 16), -EINVAL);
	invalid[1] = 9;
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, invalid, 8), -EINVAL);
	invalid[1] = 7;
	invalid[2] = 3;
	invalid[7] = 1;
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, invalid, 8), -EINVAL);
	KUNIT_EXPECT_EQ(test, f->sends, 0);
	f->send_error = -EPIPE;
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, ws73_ble_test_record, 8), -EPIPE);
	KUNIT_EXPECT_EQ(test, f->fault, -EPIPE);
	f->send_error = 0;
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, ws73_ble_test_record, 8), -EPIPE);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->adev), -EPIPE);
	KUNIT_EXPECT_EQ(test, f->sends, 1);
	KUNIT_EXPECT_EQ(test, f->faults, 1);
}

static int ws73_ble_operation_thread(void *context)
{
	struct ws73_ble_fixture *f = context;

	if (f->block_start)
		f->operation_result = ws73_ble_start(f->adev);
	else
		f->operation_result = ws73_ble_send(f->adev, ws73_ble_test_record, 8);
	complete(&f->operation_done);
	wait_event_interruptible(f->stop, kthread_should_stop());
	return 0;
}

static int ws73_ble_removal_thread(void *context)
{
	struct ws73_ble_fixture *f = context;

	ws73_ble_remove(f->adev);
	complete(&f->removed);
	wait_event_interruptible(f->stop, kthread_should_stop());
	return 0;
}

static struct task_struct *ws73_ble_test_thread(int (*fn)(void *), void *context)
{
	struct task_struct *task;

	task = kthread_create(fn, context, "ws73-ble-drain");
	if (IS_ERR(task))
		return NULL;
	get_task_struct(task);
	wake_up_process(task);
	return task;
}

static void ws73_ble_remove_inflight(struct kunit *test, bool starting)
{
	struct ws73_ble_fixture *f = test->priv;
	unsigned long waited;
	u8 event = 4;

	KUNIT_ASSERT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), 0);
	if (!starting)
		KUNIT_ASSERT_EQ(test, ws73_ble_start(f->adev), 0);
	f->block_start = starting;
	f->block_send = !starting;
	f->held = get_device(&f->adev->dev);
	f->operator = ws73_ble_test_thread(ws73_ble_operation_thread, f);
	KUNIT_ASSERT_NOT_NULL(test, f->operator);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->entered, HZ), 0UL);
	f->remover = ws73_ble_test_thread(ws73_ble_removal_thread, f);
	KUNIT_ASSERT_NOT_NULL(test, f->remover);
	/* The terminal callback proves remove has started, while the backend
	 * remains blocked. New binding and RX must already be closed.
	 */
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->faulted, HZ), 0UL);
	KUNIT_EXPECT_EQ(test, f->fault, -ENODEV);
	KUNIT_EXPECT_EQ(test, ws73_ble_bind(f->adev, &ws73_ble_test_client, f), -ENODEV);
	ws73_ble_receive(f->adev, &event, 1);
	KUNIT_EXPECT_EQ(test, f->receives, 0);
	waited = wait_for_completion_timeout(&f->removed, msecs_to_jiffies(20));
	KUNIT_EXPECT_EQ(test, waited, 0UL);
	KUNIT_EXPECT_FALSE(test, completion_done(&f->operation_done));
	complete_all(&f->release);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->operation_done, HZ), 0UL);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->removed, HZ), 0UL);
	/* A completed local TX can succeed; start may not publish enabled after
	 * the terminal fault. The extra reference permits checks after deletion.
	 */
	KUNIT_EXPECT_EQ(test, f->operation_result, starting ? -ENODEV : 0);
	KUNIT_EXPECT_EQ(test, f->starts, 1);
	KUNIT_EXPECT_EQ(test, f->sends, starting ? 0 : 1);
	KUNIT_EXPECT_EQ(test, f->faults, 1);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->adev), -ENODEV);
	KUNIT_EXPECT_EQ(test, ws73_ble_stop(f->adev), -ENODEV);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->adev, ws73_ble_test_record, 8), -ENODEV);
	KUNIT_EXPECT_EQ(test, f->stops, 0);
}

static void ws73_ble_remove_send_test(struct kunit *test)
{
	ws73_ble_remove_inflight(test, false);
}

static void ws73_ble_remove_start_test(struct kunit *test)
{
	ws73_ble_remove_inflight(test, true);
}

static struct kunit_case ws73_ble_cases[] = {
	KUNIT_CASE(ws73_ble_lifecycle_test),
	KUNIT_CASE(ws73_ble_failure_release_test),
	KUNIT_CASE(ws73_ble_send_failure_test),
	KUNIT_CASE(ws73_ble_remove_send_test),
	KUNIT_CASE(ws73_ble_remove_start_test),
	{}
};

static struct kunit_suite ws73_ble_suite = {
	.name = "ws73-ble-endpoint",
	.init = ws73_ble_test_init,
	.exit = ws73_ble_test_exit,
	.test_cases = ws73_ble_cases,
};

kunit_test_suite(ws73_ble_suite);
