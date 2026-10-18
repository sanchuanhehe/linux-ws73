// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/device.h>
#include <kunit/test.h>
#include <linux/unaligned.h>

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
	const struct btws73_chip_identity expected_chip = { .package = 0, .bus = 0 };
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
				      &expected_chip, deadline, &session);
		KUNIT_EXPECT_EQ(test, ret, expected);
		/* Even valid replacement arguments must not revive a failed attempt. */
		board = (struct ws73_boot_board) { .xo_coarse = 1, .xo_fine = 2 };
		io.receive = ws73_loader_receive;
		guard.cancelled = false;
		KUNIT_EXPECT_EQ(test,
				ws73_boot_start(dev, &io, &board, &expected_chip, 100, &session),
				expected);
		KUNIT_EXPECT_EQ(test, guard.transfers, 0);
		KUNIT_EXPECT_EQ(test, session.progress.preparation_writes, 0);
	}
}

struct ws73_loader_identity {
	unsigned int transfers;
	u32 raw;
};

static bool ws73_loader_identity_cancelled(void *context)
{
	return false;
}

static int ws73_loader_identity_send(void *context, const u8 *data, size_t size,
				     unsigned int timeout)
{
	struct ws73_loader_identity *identity = context;
	static const char readm[] = "READM 0x40019000 4 ";

	identity->transfers++;
	if (identity->transfers != 1 || size != sizeof(readm) - 1 ||
	    memcmp(data, readm, size))
		return -EIO;
	return size;
}

static int ws73_loader_identity_receive(void *context, u8 *data, size_t size,
					unsigned int timeout)
{
	struct ws73_loader_identity *identity = context;

	identity->transfers++;
	if (identity->transfers != 2 || size < sizeof(u32))
		return -EIO;
	put_unaligned_le32(identity->raw, data);
	return sizeof(u32);
}

static void ws73_loader_identity_mismatch_test(struct kunit *test)
{
	static const struct {
		u8 expected_package;
		u32 raw;
	} cases[] = {
		{ 0, 0x0a }, /* Explicit U board, live E/USB. */
		{ 2, 0x00 }, /* Explicit E board, live U/USB, including valid zero. */
		{ 2, 0x0c }, /* Alternate U encoding is still not E. */
		{ 0, 0x04 }, { 2, 0x04 }, /* SDIO cannot use USB board inputs. */
		{ 0, 0x28 }, { 2, 0x28 }, /* E/UART cannot use USB board inputs. */
	};
	const struct ws73_boot_board board = { .xo_coarse = 1, .xo_fine = 2 };
	struct device *dev = kunit_device_register(test, "ws73-loader-identity");
	unsigned int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct ws73_boot_session session = {};
		struct ws73_loader_identity identity = { .raw = cases[i].raw };
		struct btws73_chip_identity expected = { .package = cases[i].expected_package };
		const struct ws73_boot_io io = {
			.context = &identity, .now_ms = ws73_loader_now,
			.cancelled = ws73_loader_identity_cancelled,
			.send = ws73_loader_identity_send, .receive = ws73_loader_identity_receive,
			.wait_ready = ws73_loader_wait,
		};

		KUNIT_EXPECT_EQ(test,
				ws73_boot_start(dev, &io, &board, &expected, 100, &session),
				-ENODEV);
		KUNIT_EXPECT_EQ(test, session.stage, WS73_SESSION_IDENTITY);
		KUNIT_EXPECT_EQ(test, session.observation.raw, cases[i].raw);
		KUNIT_EXPECT_EQ(test, session.progress.preparation_writes, 0);
		KUNIT_EXPECT_EQ(test, identity.transfers, 2);
		/* Replacing the expected identity cannot turn failure into a retry. */
		expected = session.observation.chip;
		KUNIT_EXPECT_EQ(test,
				ws73_boot_start(dev, &io, &board, &expected, 100, &session),
				-ENODEV);
		KUNIT_EXPECT_EQ(test, identity.transfers, 2);
	}
}

static void ws73_loader_expected_admission_test(struct kunit *test)
{
	static const struct btws73_chip_identity unsupported[] = {
		{ .package = 1, .bus = 1 }, { .package = 2, .bus = 2 },
		{ .package = 0, .bus = 1 }, { .package = 3, .bus = 0 },
	};
	const struct ws73_boot_board board = { .xo_coarse = 1, .xo_fine = 2 };
	struct device *dev = kunit_device_register(test, "ws73-loader-expected");
	unsigned int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	for (i = 0; i <= ARRAY_SIZE(unsupported); i++) {
		const struct btws73_chip_identity *expected =
			i < ARRAY_SIZE(unsupported) ? &unsupported[i] : NULL;
		struct ws73_boot_session session = {};
		struct ws73_loader_guard guard = {};
		const struct ws73_boot_io io = {
			.context = &guard, .now_ms = ws73_loader_now,
			.cancelled = ws73_loader_cancelled, .send = ws73_loader_send,
			.receive = ws73_loader_receive, .wait_ready = ws73_loader_wait,
		};

		KUNIT_EXPECT_EQ(test,
				ws73_boot_start(dev, &io, &board, expected, 100, &session),
				expected ? -EOPNOTSUPP : -EINVAL);
		KUNIT_EXPECT_EQ(test, session.stage, WS73_SESSION_NEW);
		KUNIT_EXPECT_EQ(test, guard.transfers, 0);
		KUNIT_EXPECT_EQ(test, session.progress.preparation_writes, 0);
	}
}

static struct kunit_case ws73_loader_cases[] = {
	KUNIT_CASE(ws73_loader_preflight_test),
	KUNIT_CASE(ws73_loader_identity_mismatch_test),
	KUNIT_CASE(ws73_loader_expected_admission_test),
	{}
};

static struct kunit_suite ws73_loader_suite = {
	.name = "ws73-boot-loader",
	.test_cases = ws73_loader_cases,
};

kunit_test_suite(ws73_loader_suite);
