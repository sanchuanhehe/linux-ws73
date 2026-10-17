// SPDX-License-Identifier: GPL-2.0-only
/* Explicit synthetic firmware replies exercise the production link backend. */
#include <kunit/test.h>
#include <linux/unaligned.h>

#include "ble-link.h"

struct ws73_link_fixture {
	struct kunit *test;
	struct device *parent;
	struct ws73_ble_link link;
	struct ws73_startup startup;
	struct ws73_usb_tx tx;
	u64 now;
	unsigned int controls, bulks, receives, faults, transport_faults;
	int fault;
	bool short_control, short_bulk, abort_probe;
	u8 received[4];
};

static const u8 ws73_link_record[] = { 0, 7, 4, 0, 1, 3, 12, 0 };

static u64 ws73_link_now(void *context)
{
	return ((struct ws73_link_fixture *)context)->now;
}

static bool ws73_link_cancelled(void *context)
{
	return false;
}

static int ws73_link_bulk(void *context, const u8 *data, size_t length, unsigned int timeout)
{
	struct ws73_link_fixture *f = context;

	f->bulks++;
	KUNIT_EXPECT_GT(f->test, timeout, 0);
	KUNIT_EXPECT_LE(f->test, timeout, 2000);
	if (length == 864) {
		KUNIT_EXPECT_EQ(f->test, data[65], 10);
		ws73_ble_link_notify(&f->link, WS73_NOTIFY_CUSTOMIZED, f->now);
	} else if (length == 128) {
		KUNIT_EXPECT_MEMEQ(f->test, data + 64, ws73_link_record, 8);
		KUNIT_EXPECT_PTR_EQ(f->test, memchr_inv(data + 72, 0, 56), NULL);
	} else {
		KUNIT_EXPECT_EQ(f->test, length, 36);
		KUNIT_EXPECT_EQ(f->test, get_unaligned_le32(data), 3);
	}
	return f->short_bulk ? length - 1 : length;
}

static int ws73_link_control(void *context, const u8 command[4], unsigned int timeout)
{
	struct ws73_link_fixture *f = context;
	u32 action = get_unaligned_le32(command);

	f->controls++;
	KUNIT_EXPECT_TRUE(f->test, action == 25 || action == 26);
	KUNIT_EXPECT_GT(f->test, timeout, 0);
	KUNIT_EXPECT_LE(f->test, timeout, 2000);
	ws73_ble_link_notify(&f->link, action == 25 ? WS73_NOTIFY_BLE_OPEN :
			     WS73_NOTIFY_BLE_CLOSE, f->now);
	if (f->startup.state == WS73_STARTUP_ENABLE)
		ws73_ble_link_notify(&f->link, WS73_NOTIFY_BOOT, f->now);
	return f->short_control ? 3 : 4;
}

static void ws73_link_transport_fault(void *context, int error)
{
	struct ws73_link_fixture *f = context;

	f->transport_faults++;
	ws73_ble_link_abort(&f->link, error);
}

static void ws73_link_receive(void *context, const u8 *data, size_t length)
{
	struct ws73_link_fixture *f = context;

	f->receives++;
	if (length == sizeof(f->received))
		memcpy(f->received, data, length);
}

static void ws73_link_fault(void *context, int error)
{
	struct ws73_link_fixture *f = context;

	f->faults++;
	f->fault = error;
}

static const struct ws73_ble_client ws73_link_client = {
	.receive = ws73_link_receive,
	.fault = ws73_link_fault,
};

static int ws73_link_test_init(struct kunit *test)
{
	struct ws73_link_fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	struct ws73_ble_control_io control;
	struct ws73_usb_tx_io bulk;

	if (!f)
		return -ENOMEM;
	f->test = test;
	f->parent = root_device_register(test->name);
	if (IS_ERR(f->parent))
		return PTR_ERR(f->parent);
	test->priv = f;
	ws73_startup_init(&f->startup);
	ws73_usb_tx_init(&f->tx);
	bulk = (struct ws73_usb_tx_io) {
		.context = f, .now_ms = ws73_link_now,
		.cancelled = ws73_link_cancelled, .send = ws73_link_bulk,
	};
	control = (struct ws73_ble_control_io) {
		.context = f, .now_ms = ws73_link_now,
		.cancelled = ws73_link_cancelled, .send = ws73_link_control,
	};
	ws73_ble_link_init(&f->link, &f->startup, &f->tx, &bulk, &control,
			   ws73_link_transport_fault, f);
	return 0;
}

static void ws73_link_test_exit(struct kunit *test)
{
	struct ws73_link_fixture *f = test->priv;

	ws73_ble_link_remove(&f->link);
	root_device_unregister(f->parent);
}

static int ws73_link_startup(struct ws73_link_fixture *f)
{
	const struct ws73_interrupt bsp = { .events = WS73_INTERRUPT_BSP_READY };
	/* Explicit synthetic parameters, never a physical board default. */
	const struct ws73_customization parameters = {};
	int ret;

	ret = ws73_startup_arm(&f->startup, f->now, f->now + 12000);
	if (!ret)
		ret = ws73_startup_interrupt(&f->startup, &bsp, f->now);
	if (!ret)
		ret = ws73_startup_run(&f->startup, &f->tx, &f->link.bulk,
				       &f->link.control, &parameters);
	return ret;
}

static int ws73_link_publish(struct ws73_link_fixture *f)
{
	int ret = ws73_link_startup(f);

	if (!ret)
		ret = ws73_ble_control_set(&f->startup.ble, &f->link.control, false, f->now + 2000);
	/* Synchronous model has no pending data; no hardware drain is inferred. */
	if (!ret)
		ret = ws73_ble_link_publish(&f->link, f->parent);
	if (!ret)
		ret = ws73_ble_bind(f->link.child, &ws73_link_client, f);
	return ret;
}

static void ws73_link_feed(struct kunit *test, struct ws73_link_fixture *f)
{
	u8 wire[100] = { 2, 0, 0, 0, 100, 0, 0, 0, 7, 0, 0, 0, 8 };
	const u8 event[] = { 0, 7, 4, 0, 4, 0xff, 1, 0x73 };
	struct ws73_usb_rx rx;

	memcpy(wire + 92, event, 8);
	KUNIT_ASSERT_EQ(test, ws73_usb_aggregate_decode(wire, sizeof(wire), &rx), 0);
	ws73_ble_link_receive(&f->link, &rx);
	memset(wire, 0xee, sizeof(wire));
}

static void ws73_link_admission_test(struct kunit *test)
{
	struct ws73_link_fixture *f = test->priv;

	KUNIT_EXPECT_EQ(test, ws73_ble_link_publish(&f->link, f->parent), -EAGAIN);
	KUNIT_EXPECT_EQ(test, f->controls, 0);
	ws73_link_feed(test, f);
	KUNIT_EXPECT_EQ(test, f->receives, 0);
	KUNIT_ASSERT_EQ(test, ws73_link_startup(f), 0);
	/* DONE leaves BLE enabled; publication still requires acknowledged stop. */
	KUNIT_EXPECT_EQ(test, ws73_ble_link_publish(&f->link, f->parent), -EAGAIN);
	KUNIT_EXPECT_EQ(test, f->controls, 1);
	ws73_ble_link_abort(&f->link, -EIO);
	KUNIT_EXPECT_EQ(test, ws73_ble_link_publish(&f->link, f->parent), -EIO);
	KUNIT_EXPECT_PTR_EQ(test, f->link.child, NULL);
}

static void ws73_link_lifecycle_test(struct kunit *test)
{
	struct ws73_link_fixture *f = test->priv;
	struct auxiliary_device *held;
	const u8 event[] = { 4, 0xff, 1, 0x73 };

	KUNIT_ASSERT_EQ(test, ws73_link_publish(f), 0);
	KUNIT_EXPECT_EQ(test, f->controls, 2);
	KUNIT_EXPECT_EQ(test, ws73_ble_link_publish(&f->link, f->parent), -EALREADY);
	ws73_link_feed(test, f);
	KUNIT_EXPECT_EQ(test, f->receives, 0);
	KUNIT_ASSERT_EQ(test, ws73_ble_start(f->link.child), 0);
	KUNIT_EXPECT_EQ(test, f->controls, 3);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->link.child, ws73_link_record, 8), 0);
	KUNIT_EXPECT_EQ(test, f->bulks, 6); /* custom + switch/reset/BLE */
	ws73_link_feed(test, f);
	KUNIT_EXPECT_EQ(test, f->receives, 1);
	KUNIT_EXPECT_MEMEQ(test, f->received, event, sizeof(event));
	KUNIT_ASSERT_EQ(test, ws73_ble_stop(f->link.child), 0);
	KUNIT_EXPECT_EQ(test, f->controls, 4);
	ws73_link_feed(test, f);
	KUNIT_EXPECT_EQ(test, f->receives, 1);
	held = f->link.child;
	get_device(&held->dev);
	ws73_ble_link_remove(&f->link);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(held), -ENODEV);
	KUNIT_EXPECT_EQ(test, ws73_ble_link_publish(&f->link, f->parent), -ENODEV);
	KUNIT_EXPECT_EQ(test, f->controls, 4);
	put_device(&held->dev);
}

static void ws73_link_action_failure_test(struct kunit *test)
{
	struct ws73_link_fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, ws73_link_publish(f), 0);
	f->short_control = true;
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->link.child), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, f->fault, -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, f->faults, 1);
	KUNIT_EXPECT_EQ(test, f->transport_faults, 0);
	KUNIT_EXPECT_EQ(test, f->startup.state, WS73_STARTUP_DONE);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->link.child), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, f->controls, 3);
}

static void ws73_link_transport_failure_test(struct kunit *test)
{
	struct ws73_link_fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, ws73_link_publish(f), 0);
	KUNIT_ASSERT_EQ(test, ws73_ble_start(f->link.child), 0);
	f->short_bulk = true;
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->link.child, ws73_link_record, 8), -EREMOTEIO);
	KUNIT_EXPECT_EQ(test, f->transport_faults, 1);
	KUNIT_EXPECT_EQ(test, f->faults, 1);
	KUNIT_EXPECT_EQ(test, f->fault, -EREMOTEIO);
	KUNIT_EXPECT_EQ(test, f->tx.failure, -EREMOTEIO);
	ws73_link_feed(test, f);
	KUNIT_EXPECT_EQ(test, f->receives, 0);
	KUNIT_EXPECT_EQ(test, ws73_ble_send(f->link.child, ws73_link_record, 8), -EREMOTEIO);
	KUNIT_EXPECT_EQ(test, f->transport_faults, 1);
}

static int ws73_link_probe(struct auxiliary_device *adev,
			   const struct auxiliary_device_id *id)
{
	struct ws73_link_fixture *f = dev_get_drvdata(adev->dev.parent);
	int ret;

	if (!f)
		return -ENODEV;
	KUNIT_EXPECT_PTR_EQ(f->test, f->link.child, adev);
	ret = ws73_ble_bind(adev, &ws73_link_client, f);
	if (!ret)
		ret = ws73_ble_start(adev);
	if (ret) {
		ws73_ble_unbind(adev);
		return ret;
	}
	/* This RX occurs inside auxiliary_device_add(), before it returns. */
	ws73_link_feed(f->test, f);
	if (f->abort_probe)
		ws73_ble_link_abort(&f->link, -ECANCELED);
	return 0;
}

static void ws73_link_probe_remove(struct auxiliary_device *adev)
{
	ws73_ble_unbind(adev);
}

static const struct auxiliary_device_id ws73_link_ids[] = {
	{ .name = "ws73_core.ble" },
	{}
};

static struct auxiliary_driver ws73_link_driver = {
	.name = "ws73-link-test",
	.probe = ws73_link_probe,
	.remove = ws73_link_probe_remove,
	.id_table = ws73_link_ids,
};

static void ws73_link_probe_case(struct kunit *test, bool abort)
{
	struct ws73_link_fixture *f = test->priv;
	int ret;

	KUNIT_ASSERT_EQ(test, ws73_link_startup(f), 0);
	ret = ws73_ble_control_set(&f->startup.ble, &f->link.control, false, f->now + 2000);
	KUNIT_ASSERT_EQ(test, ret, 0);
	f->abort_probe = abort;
	dev_set_drvdata(f->parent, f);
	KUNIT_ASSERT_EQ(test, auxiliary_driver_register(&ws73_link_driver), 0);
	ret = ws73_ble_link_publish(&f->link, f->parent);
	KUNIT_EXPECT_EQ(test, ret, abort ? -ECANCELED : 0);
	KUNIT_EXPECT_EQ(test, f->receives, 1);
	KUNIT_EXPECT_EQ(test, f->controls, 3);
	if (abort) {
		KUNIT_EXPECT_PTR_EQ(test, f->link.child, NULL);
		KUNIT_EXPECT_EQ(test, f->fault, -ECANCELED);
		KUNIT_EXPECT_EQ(test, ws73_ble_link_publish(&f->link, f->parent), -ECANCELED);
	} else {
		KUNIT_EXPECT_NOT_NULL(test, f->link.child);
		ws73_ble_link_remove(&f->link);
	}
	auxiliary_driver_unregister(&ws73_link_driver);
	dev_set_drvdata(f->parent, NULL);
}

static void ws73_link_immediate_probe_test(struct kunit *test)
{
	ws73_link_probe_case(test, false);
}

static void ws73_link_aborted_probe_test(struct kunit *test)
{
	ws73_link_probe_case(test, true);
}

static void ws73_link_deadline_test(struct kunit *test)
{
	struct ws73_link_fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, ws73_link_publish(f), 0);
	f->now = U64_MAX;
	KUNIT_EXPECT_EQ(test, ws73_ble_start(f->link.child), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, f->controls, 2);
	KUNIT_EXPECT_EQ(test, f->transport_faults, 0);
	KUNIT_EXPECT_EQ(test, f->fault, -ETIMEDOUT);
}

static struct kunit_case ws73_link_cases[] = {
	KUNIT_CASE(ws73_link_admission_test),
	KUNIT_CASE(ws73_link_lifecycle_test),
	KUNIT_CASE(ws73_link_action_failure_test),
	KUNIT_CASE(ws73_link_transport_failure_test),
	KUNIT_CASE(ws73_link_immediate_probe_test),
	KUNIT_CASE(ws73_link_aborted_probe_test),
	KUNIT_CASE(ws73_link_deadline_test),
	{}
};

static struct kunit_suite ws73_link_suite = {
	.name = "ws73-ble-link",
	.init = ws73_link_test_init,
	.exit = ws73_link_test_exit,
	.test_cases = ws73_link_cases,
};

kunit_test_suite(ws73_link_suite);
