// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/device.h>
#include <linux/err.h>

#include "ble.h"

struct ws73_ble_fixture {
	struct device *parent;
	struct auxiliary_device *adev;
	unsigned int starts, stops, sends, receives, faults;
	int action_error, send_error, fault;
	u8 received;
};

static int ws73_ble_test_set(void *context, bool enabled)
{
	struct ws73_ble_fixture *f = context;

	if (enabled)
		f->starts++;
	else
		f->stops++;
	return f->action_error;
}

static int ws73_ble_test_send(void *context, const u8 *data, size_t length)
{
	struct ws73_ble_fixture *f = context;

	f->sends++;
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

	if (f->adev)
		ws73_ble_remove(f->adev);
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

static struct kunit_case ws73_ble_cases[] = {
	KUNIT_CASE(ws73_ble_lifecycle_test),
	KUNIT_CASE(ws73_ble_failure_release_test),
	KUNIT_CASE(ws73_ble_send_failure_test),
	{}
};

static struct kunit_suite ws73_ble_suite = {
	.name = "ws73-ble-endpoint",
	.init = ws73_ble_test_init,
	.exit = ws73_ble_test_exit,
	.test_cases = ws73_ble_cases,
};

kunit_test_suite(ws73_ble_suite);
