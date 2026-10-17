// SPDX-License-Identifier: GPL-2.0-only
/* Real auxiliary/HCI binding with a deliberately failing synthetic backend. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/jiffies.h>

#include "ble.h"

struct ws73_hci_fixture {
	struct completion attempted;
	unsigned int starts;
	unsigned int sends;
};

static int ws73_hci_test_action(void *context, bool enabled)
{
	struct ws73_hci_fixture *f = context;

	if (enabled) {
		f->starts++;
		complete(&f->attempted);
	}
	/* No successful action or HCI command completion is fabricated. */
	return -EIO;
}

static int ws73_hci_test_send(void *context, const u8 *data, size_t length)
{
	struct ws73_hci_fixture *f = context;

	f->sends++;
	return -EIO;
}

static const struct ws73_ble_backend ws73_hci_test_backend = {
	.set_enabled = ws73_hci_test_action,
	.send = ws73_hci_test_send,
};

static int ws73_hci_count_child(struct device *dev, void *context)
{
	unsigned int *count = context;

	(*count)++;
	return 0;
}

static void ws73_hci_failed_enable_test(struct kunit *test)
{
	struct ws73_hci_fixture *f;
	struct auxiliary_device *adev;
	struct device *parent;
	unsigned long deadline;
	unsigned int children;
	int ret;

	f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, f);
	init_completion(&f->attempted);
	parent = root_device_register("ws73-hci-failed-enable");
	KUNIT_ASSERT_FALSE(test, IS_ERR(parent));
	adev = ws73_ble_add(parent, &ws73_hci_test_backend, f);
	if (IS_ERR(adev)) {
		KUNIT_FAIL(test, "auxiliary registration failed: %ld", PTR_ERR(adev));
		goto parent_remove;
	}
	/* The production consumer registers HCI, whose power-on calls open. */
	KUNIT_EXPECT_TRUE(test, wait_for_completion_timeout(&f->attempted, 5 * HZ));
	KUNIT_EXPECT_EQ(test, f->starts, 1);
	deadline = jiffies + 5 * HZ;
	do {
		children = 0;
		device_for_each_child(&adev->dev, &children, ws73_hci_count_child);
		if (!children)
			break;
		msleep(20);
	} while (time_before(jiffies, deadline));
	KUNIT_EXPECT_EQ(test, children, 0);
	KUNIT_EXPECT_EQ(test, f->sends, 0);
	KUNIT_EXPECT_NOT_NULL(test, adev->dev.driver);
	/* Unbinding/rebinding the consumer must not clear the parent's fault. */
	device_release_driver(&adev->dev);
	ret = device_attach(&adev->dev);
	KUNIT_EXPECT_LE(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, adev->dev.driver, NULL);
	KUNIT_EXPECT_EQ(test, ws73_ble_start(adev), -EIO);
	KUNIT_EXPECT_EQ(test, f->starts, 1);
	ws73_ble_remove(adev);
parent_remove:
	root_device_unregister(parent);
}

static struct kunit_case ws73_hci_cases[] = {
	KUNIT_CASE(ws73_hci_failed_enable_test),
	{}
};

static struct kunit_suite ws73_hci_suite = {
	.name = "ws73-hci-lifecycle",
	.test_cases = ws73_hci_cases,
};

kunit_test_suite(ws73_hci_suite);
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
