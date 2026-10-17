// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/device.h>
#include <kunit/test.h>

#include "boot-loader.h"

struct ws73_loader_guard {
	unsigned int transfers;
	bool cancelled;
};

static u64 ws73_loader_now(void *context)
{
	return 10;
}

static bool ws73_loader_cancelled(void *context)
{
	return ((struct ws73_loader_guard *)context)->cancelled;
}

static int ws73_loader_send(void *context, const u8 *data, size_t size,
			    unsigned int timeout)
{
	((struct ws73_loader_guard *)context)->transfers++;
	return -EIO;
}

static int ws73_loader_receive(void *context, u8 *data, size_t size,
			       unsigned int timeout)
{
	((struct ws73_loader_guard *)context)->transfers++;
	return -EIO;
}

static void ws73_loader_wait(void *context)
{
	((struct ws73_loader_guard *)context)->transfers++;
}

static void ws73_loader_preflight_test(struct kunit *test)
{
	struct device *dev = kunit_device_register(test, "ws73-loader-guard");
	unsigned int scenario;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	for (scenario = 0; scenario < 8; scenario++) {
		struct ws73_boot_session session = {};
		struct ws73_loader_guard guard = {};
		struct ws73_boot_board board = { .xo_coarse = 1, .xo_fine = 2 };
		struct ws73_boot_io io = {
			.context = &guard, .now_ms = ws73_loader_now,
			.cancelled = ws73_loader_cancelled, .send = ws73_loader_send,
			.receive = ws73_loader_receive, .wait_ready = ws73_loader_wait,
		};
		u64 deadline = 100;
		int expected, ret;

		if (scenario < 2) {
			if (!scenario)
				board.xo_coarse = 16;
			else
				board.xo_fine = 128;
			expected = -ERANGE;
		} else if (scenario < 4) {
			if (scenario == 3)
				io.receive = NULL;
			expected = -EINVAL;
		} else if (scenario == 4) {
			deadline = 10;
			expected = -ETIMEDOUT;
		} else if (scenario == 5) {
			guard.cancelled = true;
			expected = -ENODEV;
		} else if (scenario == 6) {
			session.error = -EPIPE;
			expected = -EPIPE;
		} else {
			session.stage = WS73_SESSION_WAIT_RUNTIME;
			expected = -EALREADY;
		}
		ret = ws73_boot_start(scenario == 2 ? NULL : dev, &io, &board,
				      deadline, &session);
		KUNIT_EXPECT_EQ(test, ret, expected);
		/* Even valid replacement arguments must not revive a failed attempt. */
		board = (struct ws73_boot_board) { .xo_coarse = 1, .xo_fine = 2 };
		io.receive = ws73_loader_receive;
		guard.cancelled = false;
		KUNIT_EXPECT_EQ(test, ws73_boot_start(dev, &io, &board, 100, &session), expected);
		KUNIT_EXPECT_EQ(test, guard.transfers, 0);
		KUNIT_EXPECT_EQ(test, session.progress.preparation_writes, 0);
	}
}

static struct kunit_case ws73_loader_cases[] = {
	KUNIT_CASE(ws73_loader_preflight_test),
	{}
};

static struct kunit_suite ws73_loader_suite = {
	.name = "ws73-boot-loader",
	.test_cases = ws73_loader_cases,
};

kunit_test_suite(ws73_loader_suite);
