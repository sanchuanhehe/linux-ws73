// SPDX-License-Identifier: GPL-2.0-only
#include <crypto/sha2.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/hex.h>
#include <linux/module.h>

#include "boot.h"

static void ws73_profiles_test(struct kunit *test)
{
	struct btws73_chip_identity chip;
	const struct ws73_fw_profile *profile;
	unsigned int package, bus;

	KUNIT_EXPECT_EQ(test, PTR_ERR(ws73_fw_profile(NULL)), -EINVAL);
	for (bus = 0; bus <= U8_MAX; bus++) {
		for (package = 0; package <= U8_MAX; package++) {
			chip = (struct btws73_chip_identity) { package, bus };
			profile = ws73_fw_profile(&chip);
			if (!bus && (package == 0 || package == 2)) {
				KUNIT_ASSERT_FALSE(test, IS_ERR(profile));
				KUNIT_EXPECT_EQ(test, profile->package, package);
				KUNIT_EXPECT_EQ(test, profile->image[0].size,
						package ? 137644 : 143956);
				KUNIT_EXPECT_EQ(test, profile->image[2].size,
						package ? 34120 : 7772);
			} else {
				KUNIT_EXPECT_EQ(test, PTR_ERR(profile), -EOPNOTSUPP);
			}
		}
	}
}

static void ws73_file_test(struct kunit *test)
{
	u8 file[67], saved[67], digest[SHA256_DIGEST_SIZE];
	char whole[65];
	struct ws73_fw_info info = { .size = sizeof(file), .address = 0x400000,
				     .sha256 = whole };
	struct ws73_fw_view before, view;
	unsigned int i;

	/* Newly generated fixture: contains no vendor firmware bytes. */
	memcpy(file + 64, "abc", 3);
	sha256(file + 64, 3, digest);
	bin2hex(file, digest, sizeof(digest));
	sha256(file, sizeof(file), digest);
	bin2hex(whole, digest, sizeof(digest));
	whole[64] = 0;
	memcpy(saved, file, sizeof(file));
	KUNIT_ASSERT_EQ(test, ws73_fw_validate(&info, file, sizeof(file), &view), 0);
	KUNIT_EXPECT_PTR_EQ(test, view.data, file + 64);
	KUNIT_EXPECT_EQ(test, view.size, 3);
	KUNIT_EXPECT_EQ(test, view.address, 0x400000);
	memset(&before, 0xa5, sizeof(before));
	for (i = 0; i < sizeof(file); i++) {
		view = before;
		KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, file, i, &view), -EMSGSIZE);
		KUNIT_EXPECT_MEMEQ(test, &view, &before, sizeof(view));
		file[i] ^= 1;
		KUNIT_EXPECT_LT(test, ws73_fw_validate(&info, file, sizeof(file), &view), 0);
		KUNIT_EXPECT_MEMEQ(test, &view, &before, sizeof(view));
		file[i] ^= 1;
	}
	file[66] ^= 1;
	sha256(file + 64, 3, digest);
	bin2hex(file, digest, sizeof(digest));
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, file, sizeof(file), &view),
			-EKEYREJECTED);
	KUNIT_EXPECT_MEMEQ(test, &view, &before, sizeof(view));
	memcpy(file, saved, sizeof(file));
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, file, sizeof(file), &view), 0);
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, file, WS73_FW_MAX_SIZE + 1, &view),
			-EMSGSIZE);
	info.address = U32_MAX;
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, file, sizeof(file), &view), -EOVERFLOW);
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(NULL, file, sizeof(file), &view), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, NULL, sizeof(file), &view), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_fw_validate(&info, file, sizeof(file), NULL), -EINVAL);
}

struct ws73_boot_mock {
	struct kunit *test;
	u64 now;
	u64 deadline;
	unsigned int calls;
	unsigned int sleeps;
	unsigned int fail_at;
	int failure;
	bool short_io;
	bool bad_reply;
	bool stop;
	bool expire_on_call;
	bool cancel_on_call;
	unsigned int step;
	const u8 *payload;
};

/* Independent golden transcript: three chunks, then two single-chunk files. */
static const struct {
	const char *command;
	const char *reply;
	size_t offset;
	size_t length;
} ws73_boot_script[] = {
	{ "FILES 1 0x400000 0x8000 0x0 " },
	{ .reply = "READY" },
	{ .offset = 0, .length = 32768 },
	{ .reply = "FILES OK" },
	{ "FILES 1 0x408000 0x8000 0x1 " },
	{ .reply = "READY" },
	{ .offset = 32768, .length = 32768 },
	{ .reply = "FILES OK" },
	{ "FILES 1 0x410000 0x1 0x2 " },
	{ .reply = "READY" },
	{ .offset = 65536, .length = 1 },
	{ .reply = "FILES OK" },
	{ "FILES 1 0x430000 0x3 0x3 " },
	{ .reply = "READY" },
	{ .offset = 0, .length = 3 },
	{ .reply = "FILES OK" },
	{ "FILES 1 0x440000 0x5 0x3 " },
	{ .reply = "READY" },
	{ .offset = 0, .length = 5 },
	{ .reply = "FILES OK" },
	{ "QUIT" },
};

static u64 ws73_mock_now(void *context)
{
	return ((struct ws73_boot_mock *)context)->now;
}

static bool ws73_mock_cancelled(void *context)
{
	return ((struct ws73_boot_mock *)context)->stop;
}

static void ws73_mock_wait(void *context)
{
	struct ws73_boot_mock *mock = context;

	mock->now += 5;
	mock->sleeps++;
}

static bool ws73_mock_fault(struct ws73_boot_mock *mock, unsigned int timeout)
{
	KUNIT_EXPECT_GT(mock->test, timeout, 0);
	KUNIT_EXPECT_LE(mock->test, timeout, 2000);
	KUNIT_EXPECT_LE(mock->test, (u64)timeout, mock->deadline - mock->now);
	mock->calls++;
	if (mock->calls != mock->fail_at)
		return false;
	if (mock->expire_on_call)
		mock->now = mock->deadline;
	if (mock->cancel_on_call)
		mock->stop = true;
	return true;
}

static int ws73_mock_send(void *context, const u8 *data, size_t len, unsigned int timeout)
{
	struct ws73_boot_mock *mock = context;
	unsigned int step = mock->step++;
	bool fault = ws73_mock_fault(mock, timeout);

	if (step >= ARRAY_SIZE(ws73_boot_script)) {
		KUNIT_FAIL(mock->test, "unexpected I/O after transcript end");
		return -EIO;
	}
	KUNIT_EXPECT_PTR_EQ(mock->test, ws73_boot_script[step].reply, NULL);
	if (ws73_boot_script[step].command) {
		const char *command = ws73_boot_script[step].command;

		KUNIT_EXPECT_EQ(mock->test, len, strlen(command));
		if (len == strlen(command))
			KUNIT_EXPECT_MEMEQ(mock->test, data, command, len);
	} else {
		KUNIT_EXPECT_EQ(mock->test, len, ws73_boot_script[step].length);
		KUNIT_EXPECT_PTR_EQ(mock->test, data,
				    mock->payload + ws73_boot_script[step].offset);
		KUNIT_EXPECT_EQ(mock->test, mock->sleeps, step / 4 + 1);
	}
	if (fault && mock->failure)
		return mock->failure;
	return fault && mock->short_io ? len - 1 : len;
}

static int ws73_mock_receive(void *context, u8 *data, size_t capacity, unsigned int timeout)
{
	struct ws73_boot_mock *mock = context;
	unsigned int step = mock->step++;
	bool fault = ws73_mock_fault(mock, timeout);
	const char *reply;
	size_t len;

	if (step >= ARRAY_SIZE(ws73_boot_script) || !ws73_boot_script[step].reply) {
		KUNIT_FAIL(mock->test, "unexpected receive, including nonexistent QUIT ACK");
		return -EIO;
	}
	reply = ws73_boot_script[step].reply;
	len = strlen(reply);
	if (capacity < len + 1)
		return -ENOSPC;
	memcpy(data, reply, len);
	data[len] = 0;
	if (fault && mock->failure)
		return mock->failure;
	if (fault && mock->bad_reply)
		data[len] = '!';
	return fault && mock->short_io ? len - 1 : len + 1;
}

static int ws73_mock_run(struct ws73_boot_mock *mock, struct ws73_boot_progress *progress)
{
	struct ws73_fw_view image[WS73_FW_COUNT] = {
		{ mock->payload, 65537, 0x400000 },
		{ mock->payload, 3, 0x430000 },
		{ mock->payload, 5, 0x440000 },
	};
	struct ws73_boot_io io = {
		.context = mock,
		.now_ms = ws73_mock_now,
		.cancelled = ws73_mock_cancelled,
		.send = ws73_mock_send,
		.receive = ws73_mock_receive,
		.wait_ready = ws73_mock_wait,
	};

	return ws73_boot_download(&io, image, mock->deadline, progress);
}

static void ws73_boot_sequence_test(struct kunit *test)
{
	struct ws73_boot_progress progress;
	struct ws73_boot_mock mock = { .test = test, .deadline = 10000 };

	mock.payload = kunit_kzalloc(test, 65537, GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, mock.payload);
	KUNIT_ASSERT_EQ(test, ws73_mock_run(&mock, &progress), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, ARRAY_SIZE(ws73_boot_script));
	KUNIT_EXPECT_EQ(test, mock.sleeps, 5);
	KUNIT_EXPECT_EQ(test, progress.stage, WS73_BOOT_WAIT_RUNTIME);
	KUNIT_EXPECT_EQ(test, progress.image, 2);
	KUNIT_EXPECT_EQ(test, progress.acknowledged, 5);
}

static void ws73_boot_faults_test(struct kunit *test)
{
	struct ws73_boot_progress progress;
	const u8 *payload = kunit_kzalloc(test, 65537, GFP_KERNEL);
	unsigned int call, mode;

	KUNIT_ASSERT_NOT_NULL(test, payload);
	/* Every I/O boundary: error, short transfer, timeout and teardown. */
	for (mode = 0; mode < 5; mode++) {
		for (call = 1; call <= ARRAY_SIZE(ws73_boot_script); call++) {
			struct ws73_boot_mock mock = {
				.test = test, .deadline = 10000, .payload = payload,
				.fail_at = call, .failure = mode == 0 ? -EPIPE : 0,
				.short_io = mode == 1, .expire_on_call = mode == 2,
				.cancel_on_call = mode == 3, .bad_reply = mode == 4,
			};
			int expected;

			if (mode == 4 && !ws73_boot_script[call - 1].reply)
				continue;
			expected = mode == 0 ? -EPIPE : mode == 2 ? -ETIMEDOUT :
				   mode == 3 ? -ENODEV : mode == 4 ? -EPROTO :
				   ws73_boot_script[call - 1].reply ? -EMSGSIZE : -EREMOTEIO;
			KUNIT_EXPECT_EQ(test, ws73_mock_run(&mock, &progress), expected);
			KUNIT_EXPECT_EQ(test, mock.calls, call);
			KUNIT_EXPECT_NE(test, progress.stage, WS73_BOOT_WAIT_RUNTIME);
		}
	}
}

static void ws73_boot_admission_test(struct kunit *test)
{
	const u8 dummy = 0;
	struct ws73_boot_progress progress;
	struct ws73_boot_mock mock = { .test = test, .deadline = 1, .payload = &dummy,
				       .now = 1 };
	struct ws73_boot_io io = {
		.context = &mock, .now_ms = ws73_mock_now, .cancelled = ws73_mock_cancelled,
		.send = ws73_mock_send, .receive = ws73_mock_receive, .wait_ready = ws73_mock_wait,
	};
	struct ws73_fw_view image[3] = {
		{ &dummy, 1, 0x400000 }, { &dummy, 1, 0x430000 }, { &dummy, 1, 0x440000 },
	};
	unsigned int i;

	KUNIT_EXPECT_EQ(test, ws73_mock_run(&mock, &progress), -ETIMEDOUT);
	mock.now = 0;
	mock.stop = true;
	KUNIT_EXPECT_EQ(test, ws73_mock_run(&mock, &progress), -ENODEV);
	mock.stop = false;
	/* READY succeeds, but the mandatory 5 ms wait exhausts the budget. */
	KUNIT_EXPECT_EQ(test, ws73_mock_run(&mock, &progress), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.calls, 2);
	KUNIT_EXPECT_EQ(test, progress.stage, WS73_BOOT_PAYLOAD);
	mock.calls = 0;
	for (i = 0; i < 3; i++) {
		image[i].size = 0;
		KUNIT_EXPECT_EQ(test, ws73_boot_download(&io, image, 1, &progress), -EINVAL);
		image[i].size = 1;
		image[i].address++;
		KUNIT_EXPECT_EQ(test, ws73_boot_download(&io, image, 1, &progress), -EINVAL);
		image[i].address--;
	}
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	KUNIT_EXPECT_EQ(test, ws73_boot_download(NULL, image, 1, &progress), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_boot_download(&io, NULL, 1, &progress), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_boot_download(&io, image, 1, NULL), -EINVAL);
}

static struct kunit_case ws73_cases[] = {
	KUNIT_CASE(ws73_profiles_test),
	KUNIT_CASE(ws73_file_test),
	KUNIT_CASE(ws73_boot_sequence_test),
	KUNIT_CASE(ws73_boot_faults_test),
	KUNIT_CASE(ws73_boot_admission_test),
	{}
};

static struct kunit_suite ws73_suite = {
	.name = "ws73-fw-boot",
	.test_cases = ws73_cases,
};

kunit_test_suite(ws73_suite);

MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
