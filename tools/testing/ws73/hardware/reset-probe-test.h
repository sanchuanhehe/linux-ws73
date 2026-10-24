/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WS73_TRIAL_RESET_PROBE_TEST_H
#define WS73_TRIAL_RESET_PROBE_TEST_H

#include "reset-probe.h"

struct ws73_reset_fixture {
	u64 now;
	u32 word;
	int send_result, receive_result;
	unsigned int sends, reads, expire_after, cancel_after;
	bool cancelled, bad;
};

static u64 ws73_reset_test_now(void *context)
{
	return ((struct ws73_reset_fixture *)context)->now;
}

static bool ws73_reset_test_cancelled(void *context)
{
	return ((struct ws73_reset_fixture *)context)->cancelled;
}

static void ws73_reset_test_advance(struct ws73_reset_fixture *f, unsigned int step)
{
	if (f->expire_after == step)
		f->now = 100;
	if (f->cancel_after == step)
		f->cancelled = true;
}

static int ws73_reset_test_send(void *context, const u8 *data, size_t size,
				unsigned int timeout)
{
	struct ws73_reset_fixture *f = context;

	if (f->sends++ || f->reads || size != 16 || !timeout || timeout > 2000 ||
	    get_unaligned_le64(data) || get_unaligned_le32(data + 8) != 0xf118 ||
	    get_unaligned_le32(data + 12) != 0x40000008)
		f->bad = true;
	ws73_reset_test_advance(f, 1);
	return f->send_result;
}

static int ws73_reset_test_receive(void *context, u8 *data, size_t size,
				   unsigned int timeout)
{
	struct ws73_reset_fixture *f = context;

	if (f->reads++ || f->sends != 1 || size != 4 || !timeout || timeout > 2000)
		f->bad = true;
	put_unaligned_le32(f->word, data);
	ws73_reset_test_advance(f, 2);
	return f->receive_result;
}

static int ws73_reset_probe_checks(void)
{
	static const struct {
		int sent, received;
		unsigned int expire, cancel;
		int result;
		unsigned int sends, reads;
	} cases[] = {
		{ 16, 4, 0, 0, 0, 1, 1 },
		{ 0, 4, 0, 0, -EREMOTEIO, 1, 0 },
		{ 15, 4, 0, 0, -EREMOTEIO, 1, 0 },
		{ 17, 4, 0, 0, -EOVERFLOW, 1, 0 },
		{ -EIO, 4, 0, 0, -EIO, 1, 0 },
		{ -ETIMEDOUT, 4, 0, 0, -ETIMEDOUT, 1, 0 },
		{ 16, 0, 0, 0, -EREMOTEIO, 1, 1 },
		{ 16, 3, 0, 0, -EREMOTEIO, 1, 1 },
		{ 16, 5, 0, 0, -EOVERFLOW, 1, 1 },
		{ 16, -EPIPE, 0, 0, -EPIPE, 1, 1 },
		{ 16, -ENODEV, 0, 0, -ENODEV, 1, 1 },
		{ 16, 4, 3, 0, -ETIMEDOUT, 0, 0 },
		{ 16, 4, 1, 0, -ETIMEDOUT, 1, 0 },
		{ 16, 4, 2, 0, -ETIMEDOUT, 1, 1 },
		{ 16, 4, 0, 3, -ENODEV, 0, 0 },
		{ 16, 4, 0, 1, -ENODEV, 1, 0 },
		{ 16, 4, 0, 2, -ENODEV, 1, 1 },
	};
	struct ws73_reset_fixture f;
	struct ws73_boot_io invalid, io = {
		.context = &f,
		.now_ms = ws73_reset_test_now,
		.cancelled = ws73_reset_test_cancelled,
		.send = ws73_reset_test_send,
		.receive = ws73_reset_test_receive,
	};
	u32 value, expected;
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		f = (struct ws73_reset_fixture) {
			.now = 10, .word = 0x12345678,
			.send_result = cases[i].sent, .receive_result = cases[i].received,
			.expire_after = cases[i].expire, .cancel_after = cases[i].cancel,
		};
		ws73_reset_test_advance(&f, 3);
		value = 0xa5a5a5a5;
		ret = ws73_trial_reset_read(&io, 100, &value);
		expected = ret ? 0xa5a5a5a5U : f.word;
		if (ret != cases[i].result || f.bad || f.sends != cases[i].sends ||
		    f.reads != cases[i].reads || value != expected)
			return -EINVAL;
	}
	/* A complete zero response is valid; a long deadline still clips each I/O. */
	f = (struct ws73_reset_fixture) { .now = 10, .send_result = 16, .receive_result = 4 };
	value = 0xa5a5a5a5;
	if (ws73_trial_reset_read(&io, U64_MAX, &value) || value || f.bad)
		return -EINVAL;
	f = (struct ws73_reset_fixture) {};
	value = 0xa5a5a5a5;
	if (ws73_trial_reset_read(NULL, 100, &value) != -EINVAL ||
	    ws73_trial_reset_read(&io, 100, NULL) != -EINVAL)
		return -EINVAL;
	for (i = 0; i < 4; i++) {
		invalid = io;
		if (i == 0)
			invalid.send = NULL;
		else if (i == 1)
			invalid.receive = NULL;
		else if (i == 2)
			invalid.now_ms = NULL;
		else
			invalid.cancelled = NULL;
		if (ws73_trial_reset_read(&invalid, 100, &value) != -EINVAL)
			return -EINVAL;
	}
	if (f.sends || f.reads || value != 0xa5a5a5a5)
		return -EINVAL;
	pr_info("RESET fixed read checks passed: %zu; no device I/O\n", ARRAY_SIZE(cases) + 7);
	return 0;
}

struct ws73_reset_attempt_fixture {
	u64 now;
	u32 word;
	unsigned int calls, fault_step, expire_after, cancel_after;
	int fault_result;
	bool cancelled, bad;
};

static u64 ws73_attempt_now(void *context)
{
	return ((struct ws73_reset_attempt_fixture *)context)->now;
}

static bool ws73_attempt_cancelled(void *context)
{
	return ((struct ws73_reset_attempt_fixture *)context)->cancelled;
}

static int ws73_attempt_advance(struct ws73_reset_attempt_fixture *f, int length)
{
	f->calls++;
	if (f->expire_after == f->calls)
		f->now = 100;
	if (f->cancel_after == f->calls)
		f->cancelled = true;
	return f->fault_step == f->calls ? f->fault_result : length;
}

static int ws73_attempt_send(void *context, const u8 *data, size_t size,
			     unsigned int timeout)
{
	struct ws73_reset_attempt_fixture *f = context;

	if (!timeout || timeout > 2000)
		f->bad = true;
	if (f->calls == 0 || f->calls == 2) {
		if (size != 16 || get_unaligned_le64(data) ||
		    get_unaligned_le32(data + 8) != 0xf118 ||
		    get_unaligned_le32(data + 12) != (f->calls ? 0x40000009 : 0x40000008))
			f->bad = true;
	} else if (f->calls == 3) {
		if (size != 4 || get_unaligned_le32(data) != (f->word | BIT(4)))
			f->bad = true;
	} else {
		f->bad = true;
	}
	return ws73_attempt_advance(f, size);
}

static int ws73_attempt_receive(void *context, u8 *data, size_t size,
				unsigned int timeout)
{
	struct ws73_reset_attempt_fixture *f = context;

	if (f->calls != 1 || size != 4 || !timeout || timeout > 2000)
		f->bad = true;
	put_unaligned_le32(f->word, data);
	return ws73_attempt_advance(f, size);
}

static int ws73_attempt_check(struct ws73_reset_attempt_fixture *f, int result,
			      unsigned int calls)
{
	const struct ws73_boot_io io = {
		.context = f,
		.now_ms = ws73_attempt_now,
		.cancelled = ws73_attempt_cancelled,
		.send = ws73_attempt_send,
		.receive = ws73_attempt_receive,
	};
	struct ws73_trial_reset_progress progress;
	bool attempted = calls == 4;
	int ret, raw;

	f->now = f->expire_after == 5 ? 100 : 10;
	f->cancelled = f->cancel_after == 5;
	ret = ws73_trial_reset_attempt(&io, 100, &progress);
	if (ret != result || f->calls != calls || f->bad ||
	    progress.data_attempted != attempted)
		return -EINVAL;
	if (calls >= 3 || result == -EALREADY) {
		if (!progress.read_valid || progress.before != f->word)
			return -EINVAL;
	} else if (progress.read_valid || progress.before) {
		return -EINVAL;
	}
	if (calls == 4) {
		raw = f->fault_step == 4 ? f->fault_result : 4;
		if (progress.transfer_result != raw || progress.stage != WS73_RESET_WRITE_DATA)
			return -EINVAL;
	}
	return 0;
}

static int ws73_reset_attempt_checks(void)
{
	struct ws73_reset_attempt_fixture f;
	struct ws73_trial_reset_progress progress, before;
	const struct ws73_boot_io io = {};
	unsigned int step, i, count = 0;
	int size, faults[4];

	for (i = 0; i < 2; i++) {
		f = (struct ws73_reset_attempt_fixture) { .word = i ? 0x12345668 : 0 };
		if (ws73_attempt_check(&f, 0, 4))
			return -EINVAL;
		count++;
	}
	f = (struct ws73_reset_attempt_fixture) { .word = BIT(4) };
	if (ws73_attempt_check(&f, -EALREADY, 2))
		return -EINVAL;
	count++;
	for (step = 1; step <= 4; step++) {
		size = step == 1 || step == 3 ? 16 : 4;
		faults[0] = -EIO;
		faults[1] = 0;
		faults[2] = size - 1;
		faults[3] = size + 1;
		for (i = 0; i < ARRAY_SIZE(faults); i++) {
			f = (struct ws73_reset_attempt_fixture) {
				.word = 0x12345668, .fault_step = step, .fault_result = faults[i],
			};
			if (ws73_attempt_check(&f, i == 0 ? -EIO :
					       i == 3 ? -EOVERFLOW : -EREMOTEIO, step))
				return -EINVAL;
			count++;
		}
	}
	for (step = 1; step <= 5; step++) {
		f = (struct ws73_reset_attempt_fixture) {
			.word = 0x12345668, .expire_after = step,
		};
		if (ws73_attempt_check(&f, -ETIMEDOUT, step == 5 ? 0 : step))
			return -EINVAL;
		count++;
		f = (struct ws73_reset_attempt_fixture) {
			.word = 0x12345668, .cancel_after = step,
		};
		if (ws73_attempt_check(&f, -ENODEV, step == 5 ? 0 : step))
			return -EINVAL;
		count++;
	}
	f = (struct ws73_reset_attempt_fixture) {
		.word = 0x12345668, .fault_step = 4, .fault_result = -ENODEV,
	};
	if (ws73_attempt_check(&f, -ENODEV, 4))
		return -EINVAL;
	count++;
	memset(&progress, 0xa5, sizeof(progress));
	before = progress;
	if (ws73_trial_reset_attempt(NULL, 100, &progress) != -EINVAL ||
	    memcmp(&progress, &before, sizeof(progress)))
		return -EINVAL;
	count++;
	if (ws73_trial_reset_attempt(&io, 100, NULL) != -EINVAL)
		return -EINVAL;
	count++;
	pr_info("RESET fixed attempt checks passed: %u; no device I/O\n", count);
	return 0;
}

#endif
