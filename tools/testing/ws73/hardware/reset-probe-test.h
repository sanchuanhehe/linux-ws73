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

#endif
