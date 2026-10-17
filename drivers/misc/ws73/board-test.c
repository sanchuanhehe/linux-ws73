// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>

#include "boot.h"

static const char * const ws73_board_commands[] = {
	"WRITEM 4 0x40017750 51 ",
	"WRITEM 4 0x40017790 0 ",
	"WRITEM 4 0x40017794 0 ",
	"WRITEM 4 0x4001775c 4390912 ",
	"WRITEM 4 0x40017760 4423680 ",
	"WRITEM 4 0x40017750 63 ",
	"WRITEM 4 0x40017750 47 ",
	"WRITEM 4 0x40017798 0 ",
	"WRITEM 4 0x40017764 4456448 ",
	"WRITEM 4 0x40017750 63 ",
	"WRITEM 4 0x40019408 513 ", /* Synthetic explicit coarse=1, fine=2. */
};

static const char * const ws73_board_files[] = {
	"FILES 1 0x400000 0x1 0x3 ",
	"FILES 1 0x430000 0x1 0x3 ",
	"FILES 1 0x440000 0x1 0x3 ",
};

struct ws73_board_mock {
	struct kunit *test;
	struct ws73_boot_io io;
	struct ws73_boot_board board;
	struct ws73_fw_view image[3];
	u64 now;
	unsigned int calls, waits, fail_at, fault;
	bool cancelled, echo;
	const u8 *first_ack;
	size_t first_ack_length;
	u8 payload;
};

static u64 ws73_board_now(void *context)
{
	return ((struct ws73_board_mock *)context)->now;
}

static bool ws73_board_cancelled(void *context)
{
	return ((struct ws73_board_mock *)context)->cancelled;
}

static int ws73_board_result(struct ws73_board_mock *mock, size_t length)
{
	mock->now++;
	if (++mock->calls != mock->fail_at)
		return length;
	switch (mock->fault) {
	case 0:
		return -EPIPE;
	case 1:
		return length - 1;
	case 2:
		mock->cancelled = true;
		break;
	case 3:
		mock->now = 100;
		break;
	case 4:
		return 513; /* Larger than the RX buffer or any command/payload. */
	}
	return length;
}

static int ws73_board_send(void *context, const u8 *data, size_t length, unsigned int timeout)
{
	struct ws73_board_mock *mock = context;
	struct kunit *test = mock->test;
	unsigned int step = mock->calls;
	const char *expected;

	KUNIT_EXPECT_GT(test, timeout, 0);
	KUNIT_EXPECT_LE(test, timeout, 100 - mock->now);
	if (step < 22) {
		KUNIT_EXPECT_EQ(test, step % 2, 0);
		expected = ws73_board_commands[step / 2];
	} else if (step == 34) {
		expected = "QUIT ";
	} else if (step < 34 && (step - 22) % 4 == 2) {
		KUNIT_EXPECT_EQ(test, length, 1);
		KUNIT_EXPECT_EQ(test, data[0], mock->payload);
		KUNIT_EXPECT_EQ(test, mock->waits, (step - 22) / 4 + 1);
		return ws73_board_result(mock, length);
	} else if (step < 34 && (step - 22) % 4 == 0) {
		expected = ws73_board_files[(step - 22) / 4];
	} else {
		KUNIT_FAIL(test, "unexpected TX or continued execution after failure");
		return -EIO;
	}
	KUNIT_EXPECT_EQ(test, length, strlen(expected));
	if (length == strlen(expected))
		KUNIT_EXPECT_MEMEQ(test, data, expected, length);
	return ws73_board_result(mock, length);
}

static int ws73_board_receive(void *context, u8 *data, size_t capacity, unsigned int timeout)
{
	struct ws73_board_mock *mock = context;
	unsigned int step = mock->calls;
	const char *reply;

	KUNIT_EXPECT_EQ(mock->test, capacity, 512);
	KUNIT_EXPECT_GT(mock->test, timeout, 0);
	if (step == 1 && mock->first_ack) {
		memcpy(data, mock->first_ack, mock->first_ack_length);
		return ws73_board_result(mock, mock->first_ack_length);
	}
	if (step < 22 && step % 2) {
		if (mock->echo) {
			int length = scnprintf(data, capacity, "WRITEM OK:%s",
					       ws73_board_commands[step / 2]);

			return ws73_board_result(mock, length + 1);
		}
		reply = "WRITEM OK";
	} else if (step < 34 && step % 2) {
		reply = (step - 22) % 4 == 1 ? "READY" : "FILES OK";
	} else {
		KUNIT_FAIL(mock->test, "unexpected RX, including nonexistent QUIT ACK");
		return -EIO;
	}
	memcpy(data, reply, strlen(reply));
	return ws73_board_result(mock, strlen(reply));
}

static void ws73_board_wait(void *context)
{
	struct ws73_board_mock *mock = context;

	mock->waits++;
	mock->now += 5;
}

static void ws73_board_mock_init(struct ws73_board_mock *mock, struct kunit *test)
{
	unsigned int i;

	memset(mock, 0, sizeof(*mock));
	mock->test = test;
	mock->payload = 0x5a;
	mock->board = (struct ws73_boot_board) { .xo_coarse = 1, .xo_fine = 2 };
	mock->io = (struct ws73_boot_io) {
		.context = mock, .now_ms = ws73_board_now, .cancelled = ws73_board_cancelled,
		.send = ws73_board_send, .receive = ws73_board_receive,
		.wait_ready = ws73_board_wait,
	};
	for (i = 0; i < 3; i++)
		mock->image[i] = (struct ws73_fw_view) {
			.data = &mock->payload, .size = 1,
			.address = i == 0 ? 0x400000 : i == 1 ? 0x430000 : 0x440000,
		};
}

static void ws73_board_sequence_test(struct kunit *test)
{
	struct ws73_board_mock mock;
	struct ws73_boot_progress progress;

	ws73_board_mock_init(&mock, test);
	KUNIT_ASSERT_EQ(test, ws73_boot_load(&mock.io, mock.image, &mock.board, 100, &progress), 0);
	KUNIT_EXPECT_EQ(test, progress.preparation_writes, 11);
	KUNIT_EXPECT_EQ(test, progress.stage, WS73_BOOT_WAIT_RUNTIME);
	KUNIT_EXPECT_EQ(test, progress.image, 2);
	KUNIT_EXPECT_EQ(test, progress.acknowledged, 1);
	KUNIT_EXPECT_EQ(test, mock.calls, 35);
	KUNIT_EXPECT_EQ(test, mock.waits, 3);
}

static void ws73_board_faults_test(struct kunit *test)
{
	struct ws73_board_mock mock;
	struct ws73_boot_progress progress;
	unsigned int step, fault;
	int expected, ret;

	for (step = 1; step <= 35; step++) {
		for (fault = 0; fault < 5; fault++) {
			ws73_board_mock_init(&mock, test);
			mock.fail_at = step;
			mock.fault = fault;
			expected = fault == 0 ? -EPIPE : fault == 2 ? -ENODEV :
				fault == 3 ? -ETIMEDOUT : step % 2 ? -EREMOTEIO : -EMSGSIZE;
			ret = ws73_boot_load(&mock.io, mock.image, &mock.board, 100, &progress);
			KUNIT_EXPECT_EQ(test, ret, expected);
			KUNIT_EXPECT_EQ(test, mock.calls, step);
			if (step <= 22) {
				KUNIT_EXPECT_EQ(test, progress.stage, WS73_BOOT_PREPARE);
				KUNIT_EXPECT_EQ(test, progress.preparation_writes, (step - 1) / 2);
				KUNIT_EXPECT_EQ(test, mock.waits, 0);
			}
			KUNIT_EXPECT_NE(test, progress.stage, WS73_BOOT_WAIT_RUNTIME);
		}
	}
}

static void ws73_board_echo_test(struct kunit *test)
{
	static const u8 ack[] = "WRITEM OK:WRITEM 4 0x40017750 51 ";
	struct ws73_boot_progress progress;
	struct ws73_board_mock mock;
	u8 padded[512] = {};
	unsigned int i;
	int ret;

	memcpy(padded, ack, sizeof(ack));
	for (i = 0; i <= sizeof(ack); i++) {
		ws73_board_mock_init(&mock, test);
		mock.echo = true;
		mock.first_ack = padded;
		mock.first_ack_length = i;
		ret = ws73_boot_load(&mock.io, mock.image, &mock.board, 100, &progress);
		if (i == 9 || i >= sizeof(ack) - 1) {
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_EQ(test, progress.preparation_writes, 11);
			KUNIT_EXPECT_EQ(test, mock.calls, 35);
		} else {
			KUNIT_EXPECT_EQ(test, ret, i < 9 ? -EMSGSIZE : -EPROTO);
			KUNIT_EXPECT_EQ(test, mock.calls, 2);
			KUNIT_EXPECT_EQ(test, progress.preparation_writes, 0);
		}
	}
	ws73_board_mock_init(&mock, test);
	mock.echo = true;
	mock.first_ack = padded;
	mock.first_ack_length = sizeof(padded);
	KUNIT_EXPECT_EQ(test, ws73_boot_load(&mock.io, mock.image, &mock.board,
					     100, &progress), 0);
}

static void ws73_board_echo_reject_test(struct kunit *test)
{
	static const u8 ack[] = "WRITEM OK:WRITEM 4 0x40017750 51 ";
	struct ws73_boot_progress progress;
	struct ws73_board_mock mock;
	u8 modified[sizeof(ack) + 1];
	unsigned int i;

	/* Every changed byte (including address/value/space and NUL suffix)
	 * must stop before the second write. A prefix match is insufficient.
	 */
	for (i = 0; i < sizeof(modified); i++) {
		memset(modified, 0, sizeof(modified));
		memcpy(modified, ack, sizeof(ack));
		modified[i] ^= 1;
		ws73_board_mock_init(&mock, test);
		mock.first_ack = modified;
		mock.first_ack_length = sizeof(modified);
		KUNIT_EXPECT_EQ(test, ws73_boot_load(&mock.io, mock.image, &mock.board,
						     100, &progress), -EPROTO);
		KUNIT_EXPECT_EQ(test, mock.calls, 2);
		KUNIT_EXPECT_EQ(test, progress.preparation_writes, 0);
		KUNIT_EXPECT_EQ(test, mock.waits, 0);
	}
}

static void ws73_board_admission_test(struct kunit *test)
{
	struct ws73_board_mock mock;
	struct ws73_boot_progress progress, before;
	unsigned int field;
	int ret;

	memset(&before, 0xa5, sizeof(before));
	for (field = 0; field < 5; field++) {
		ws73_board_mock_init(&mock, test);
		progress = before;
		if (field == 0)
			mock.board.xo_coarse = 16;
		else if (field == 1)
			mock.board.xo_fine = 128;
		else if (field == 2)
			mock.image[2].address++;
		else if (field == 3)
			mock.image[2].data = NULL;
		else
			mock.image[0].size = 0x30001;
		ret = ws73_boot_load(&mock.io, mock.image, &mock.board, 100, &progress);
		KUNIT_EXPECT_EQ(test, ret, field < 2 ? -ERANGE : -EINVAL);
		KUNIT_EXPECT_MEMEQ(test, &progress, &before, sizeof(progress));
		KUNIT_EXPECT_EQ(test, mock.calls, 0);
	}
	ws73_board_mock_init(&mock, test);
	KUNIT_EXPECT_EQ(test, ws73_boot_load(&mock.io, mock.image, NULL, 100, &progress), -EINVAL);
	mock.cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_boot_load(&mock.io, mock.image, &mock.board, 100, &progress),
			-ENODEV);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	mock.cancelled = false;
	KUNIT_EXPECT_EQ(test, ws73_boot_load(&mock.io, mock.image, &mock.board, 0, &progress),
			-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
}

static struct kunit_case ws73_board_cases[] = {
	KUNIT_CASE(ws73_board_sequence_test),
	KUNIT_CASE(ws73_board_faults_test),
	KUNIT_CASE(ws73_board_admission_test),
	KUNIT_CASE(ws73_board_echo_test),
	KUNIT_CASE(ws73_board_echo_reject_test),
	{}
};

static struct kunit_suite ws73_board_suite = {
	.name = "ws73-board-load",
	.test_cases = ws73_board_cases,
};

kunit_test_suite(ws73_board_suite);
