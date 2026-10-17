// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched/task.h>
#include <linux/unaligned.h>

#include "frw-transaction.h"

enum ws73_frw_test_mode {
	REPLY,
	ERROR_AFTER_REPLY,
	SHORT_AFTER_REPLY,
	LONG_AFTER_REPLY,
	DUPLICATE_REPLY,
	ABORT_AFTER_REPLY,
	MALFORMED_REPLY,
	NO_REPLY,
	EXPIRED_REPLY,
	REPLY_THEN_EXPIRE,
	BLOCK_SEND,
};

struct ws73_frw_fixture {
	struct ws73_frw_transaction transaction;
	struct ws73_frw_transport transport;
	struct ws73_frw_result result;
	struct completion entered;
	struct completion release;
	struct completion finished;
	struct completion shutdown_entered;
	struct completion shutdown_finished;
	struct kunit *test;
	enum ws73_frw_test_mode mode;
	u8 reply[40];
	u8 previous[40];
	unsigned int sends;
	int ret;
};

/* Independent wire fixture, not produced using the production encoder. */
static void ws73_frw_test_reply(struct ws73_frw_fixture *f, u32 sequence)
{
	static const u8 bytes[40] = {
		0x34, 0x12, 0x12, 0x20, 0,    0,    0,	  0,	0x34, 0x12,
		2,    3,    0xfb, 0xff, 0xff, 0xff, 0,	  0,	0,    0,
		0xef, 0xbe, 0xad, 0xde, 0x67, 0x45, 0x23, 1,	0,    0,
		4,    0,    4,	  0,	1,    0,    0xa1, 0xb2, 0xc3, 0xd4,
	};

	memcpy(f->reply, bytes, sizeof(bytes));
	put_unaligned_le32(sequence, f->reply + 16);
}

static int ws73_frw_test_send(void *context, const u8 *record, size_t length,
			      u64 deadline)
{
	struct ws73_frw_fixture *f = context;
	struct kunit *test = f->test;
	u32 sequence = get_unaligned_le32(record + 16);
	int ret;

	f->sends++;
	KUNIT_EXPECT_EQ(test, length, (size_t)36);
	KUNIT_EXPECT_EQ(test, record[2], 2);
	KUNIT_EXPECT_EQ(test, record[11], 1);
	KUNIT_EXPECT_NE(test, deadline, 0ULL);
	ws73_frw_test_reply(f, sequence);
	if (f->mode == BLOCK_SEND) {
		complete(&f->entered);
		if (!wait_for_completion_timeout(&f->release,
						 msecs_to_jiffies(1500)))
			return -ETIMEDOUT;
		return length;
	}
	if (f->mode == NO_REPLY)
		return length;
	if (f->mode == EXPIRED_REPLY) {
		msleep(30);
		KUNIT_EXPECT_EQ(test,
				ws73_frw_transaction_receive(&f->transaction,
							     f->reply,
							     sizeof(f->reply)),
				-ETIMEDOUT);
		return length;
	}
	if (f->mode == MALFORMED_REPLY)
		f->reply[5] = 1;
	if (f->sends > 1) {
		/* A completed command cannot satisfy the next command. */
		ret = ws73_frw_transaction_receive(&f->transaction, f->previous,
						   sizeof(f->previous));
		KUNIT_EXPECT_EQ(test, ret, -ENOENT);
	}
	ret = ws73_frw_transaction_receive(&f->transaction, f->reply,
					   sizeof(f->reply));
	KUNIT_EXPECT_EQ(test, ret, f->mode == MALFORMED_REPLY ? -EPROTO : 0);
	memcpy(f->previous, f->reply, sizeof(f->previous));
	if (f->mode == DUPLICATE_REPLY)
		KUNIT_EXPECT_EQ(test,
				ws73_frw_transaction_receive(&f->transaction,
							     f->reply,
							     sizeof(f->reply)),
				-EPROTO);
	/* RX buffers can be reused before send returns. */
	memset(f->reply, 0xa5, sizeof(f->reply));
	if (f->mode == REPLY_THEN_EXPIRE)
		msleep(30);
	if (f->mode == ERROR_AFTER_REPLY)
		return -EPIPE;
	if (f->mode == SHORT_AFTER_REPLY)
		return length - 1;
	if (f->mode == LONG_AFTER_REPLY)
		return length + 1;
	if (f->mode == ABORT_AFTER_REPLY)
		ws73_frw_transaction_abort(&f->transaction, -ENODEV);
	return length;
}

static int ws73_frw_fixture_init(struct kunit *test)
{
	struct ws73_frw_fixture *f;

	f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	test->priv = f;
	f->test = test;
	f->transport.context = f;
	f->transport.send = ws73_frw_test_send;
	ws73_frw_transaction_init(&f->transaction);
	init_completion(&f->entered);
	init_completion(&f->release);
	init_completion(&f->finished);
	init_completion(&f->shutdown_entered);
	init_completion(&f->shutdown_finished);
	memset(&f->result, 0xa5, sizeof(f->result));
	return 0;
}

static int ws73_frw_test_call(struct ws73_frw_fixture *f, unsigned int timeout)
{
	return ws73_frw_transact(&f->transaction, &f->transport, 0x1234, 2,
				 NULL, 0, 4, timeout, &f->result);
}

static void ws73_frw_expect_untouched(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;
	const u8 *bytes = (const u8 *)&f->result;
	size_t i;

	for (i = 0; i < sizeof(f->result); i++)
		KUNIT_EXPECT_EQ(test, bytes[i], 0xa5);
}

static void ws73_frw_transaction_success(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;
	unsigned int i;

	for (i = 0; i < 3; i++) {
		KUNIT_ASSERT_EQ(test, ws73_frw_test_call(f, 1000), 0);
		KUNIT_EXPECT_EQ(test, f->result.device_status, -5);
		KUNIT_EXPECT_EQ(test, f->result.length, (size_t)4);
		KUNIT_EXPECT_MEMEQ(test, f->result.data, "\xa1\xb2\xc3\xd4", 4);
		KUNIT_EXPECT_EQ(test, f->result.data[4], 0);
		KUNIT_EXPECT_EQ(test, f->result.data[WS73_FRW_DATA_MAX - 1], 0);
		KUNIT_EXPECT_EQ(test, f->transaction.next_sequence, (u64)i + 2);
	}
	KUNIT_EXPECT_EQ(test, f->sends, 3U);
}

static void ws73_frw_transaction_failures(struct kunit *test)
{
	static const struct {
		enum ws73_frw_test_mode mode;
		int error;
	} cases[] = {
		{ ERROR_AFTER_REPLY, -EPIPE },	{ SHORT_AFTER_REPLY, -EIO },
		{ LONG_AFTER_REPLY, -EIO },	{ DUPLICATE_REPLY, -EPROTO },
		{ ABORT_AFTER_REPLY, -ENODEV }, { MALFORMED_REPLY, -EPROTO },
	};
	struct ws73_frw_fixture *f = test->priv;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		/* Each test iteration represents a fresh independent object. */
		ws73_frw_transaction_init(&f->transaction);
		f->sends = 0;
		f->mode = cases[i].mode;
		KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000),
				cases[i].error);
		ws73_frw_expect_untouched(test);
		ws73_frw_transaction_abort(&f->transaction, -ECANCELED);
		KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000),
				cases[i].error);
		ws73_frw_test_reply(f, 1);
		KUNIT_EXPECT_EQ(test,
				ws73_frw_transaction_receive(&f->transaction,
							     f->reply,
							     sizeof(f->reply)),
				cases[i].error);
		KUNIT_EXPECT_EQ(test, f->sends, 1U);
	}
}

static void ws73_frw_transaction_deadline(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;

	f->mode = EXPIRED_REPLY;
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 20), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -ETIMEDOUT);
	ws73_frw_expect_untouched(test);
	KUNIT_EXPECT_EQ(test, f->sends, 1U);

	/* A timely reply cannot excuse TX returning after the same deadline. */
	ws73_frw_transaction_init(&f->transaction);
	f->sends = 0;
	f->mode = REPLY_THEN_EXPIRE;
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 20), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -ETIMEDOUT);
	ws73_frw_expect_untouched(test);
	KUNIT_EXPECT_EQ(test, f->sends, 1U);
}

static void ws73_frw_transaction_timeout(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;

	f->mode = NO_REPLY;
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 20), -ETIMEDOUT);
	ws73_frw_test_reply(f, 1);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transaction_receive(&f->transaction, f->reply,
						     sizeof(f->reply)),
			-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -ETIMEDOUT);
	ws73_frw_expect_untouched(test);
	KUNIT_EXPECT_EQ(test, f->sends, 1U);
}

static void ws73_frw_transaction_exhaustion(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;

	f->transaction.next_sequence = U32_MAX;
	KUNIT_ASSERT_EQ(test, ws73_frw_test_call(f, 1000), 0);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(f->previous + 16), U32_MAX);
	KUNIT_EXPECT_EQ(test, f->transaction.next_sequence, (u64)U32_MAX + 1);
	memset(&f->result, 0xa5, sizeof(f->result));
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -EOVERFLOW);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -EOVERFLOW);
	ws73_frw_expect_untouched(test);
	KUNIT_EXPECT_EQ(test, f->sends, 1U);
}

static void ws73_frw_transaction_validation(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;

	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 2001), -EINVAL);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transact(&f->transaction, &f->transport, 0, 16,
					  NULL, 0, 0, 1, &f->result),
			-EINVAL);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transact(&f->transaction, &f->transport, 0, 0,
					  NULL, 1, 0, 1, &f->result),
			-EINVAL);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transact(&f->transaction, &f->transport, 0, 0,
					  NULL, 0, SIZE_MAX, 1, &f->result),
			-EMSGSIZE);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transact(&f->transaction, &f->transport, 0, 0,
					  f->reply, SIZE_MAX, 0, 1, &f->result),
			-EMSGSIZE);
	KUNIT_EXPECT_EQ(test, f->transaction.next_sequence, 1ULL);
	KUNIT_EXPECT_EQ(test, f->sends, 0U);
	ws73_frw_expect_untouched(test);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), 0);
}

static int ws73_frw_test_worker(void *context)
{
	struct ws73_frw_fixture *f = context;

	f->ret = ws73_frw_test_call(f, 2000);
	complete(&f->finished);
	return 0;
}

static int ws73_frw_test_shutdown_worker(void *context)
{
	struct ws73_frw_fixture *f = context;

	complete(&f->shutdown_entered);
	ws73_frw_transaction_shutdown(&f->transaction);
	complete(&f->shutdown_finished);
	return 0;
}

static struct task_struct *ws73_frw_test_start(int (*fn)(void *), void *context)
{
	struct task_struct *task = kthread_create(fn, context, "ws73-frw-test");

	if (!IS_ERR(task)) {
		get_task_struct(task);
		wake_up_process(task);
	}
	return task;
}

static void ws73_frw_test_join(struct task_struct *task)
{
	kthread_stop(task);
	put_task_struct(task);
}

static void ws73_frw_transaction_independent_rx(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;
	struct task_struct *worker;
	unsigned long entered;

	f->mode = BLOCK_SEND;
	worker = ws73_frw_test_start(ws73_frw_test_worker, f);
	KUNIT_ASSERT_FALSE(test, IS_ERR(worker));
	entered = wait_for_completion_timeout(&f->entered,
					      msecs_to_jiffies(1000));
	KUNIT_EXPECT_NE(test, entered, 0UL);
	if (entered) {
		KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -EBUSY);
		KUNIT_EXPECT_EQ(test,
				ws73_frw_transaction_receive(&f->transaction,
							     f->reply,
							     sizeof(f->reply)),
				0);
		memset(f->reply, 0, sizeof(f->reply));
		KUNIT_EXPECT_FALSE(test, completion_done(&f->finished));
		ws73_frw_expect_untouched(test);
	}
	complete(&f->release);
	ws73_frw_test_join(worker);
	KUNIT_EXPECT_TRUE(test, completion_done(&f->finished));
	KUNIT_EXPECT_EQ(test, f->ret, 0);
	KUNIT_EXPECT_MEMEQ(test, f->result.data, "\xa1\xb2\xc3\xd4", 4);
}

static void ws73_frw_transaction_shutdown_drain(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;
	struct task_struct *worker, *shutdown;
	unsigned long entered;

	f->mode = BLOCK_SEND;
	worker = ws73_frw_test_start(ws73_frw_test_worker, f);
	KUNIT_ASSERT_FALSE(test, IS_ERR(worker));
	entered = wait_for_completion_timeout(&f->entered,
					      msecs_to_jiffies(1000));
	KUNIT_EXPECT_NE(test, entered, 0UL);
	ws73_frw_transaction_abort(&f->transaction, -ENODEV);
	shutdown =
		ws73_frw_test_start(ws73_frw_test_shutdown_worker, f);
	KUNIT_EXPECT_FALSE(test, IS_ERR(shutdown));
	if (!IS_ERR(shutdown)) {
		unsigned long limit = jiffies + msecs_to_jiffies(500);
		bool waiting = false;

		entered = wait_for_completion_timeout(&f->shutdown_entered,
						      msecs_to_jiffies(500));
		KUNIT_EXPECT_NE(test, entered, 0UL);
		do {
			waiting = READ_ONCE(shutdown->__state) == TASK_UNINTERRUPTIBLE;
			if (waiting || completion_done(&f->shutdown_finished))
				break;
			usleep_range(1000, 2000);
		} while (time_before(jiffies, limit));
		KUNIT_EXPECT_TRUE(test, waiting);
	}
	KUNIT_EXPECT_FALSE(test, completion_done(&f->shutdown_finished));
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transaction_receive(&f->transaction, f->reply,
						     sizeof(f->reply)),
			-ENODEV);
	complete(&f->release);
	ws73_frw_test_join(worker);
	if (!IS_ERR(shutdown))
		ws73_frw_test_join(shutdown);
	KUNIT_EXPECT_TRUE(test, completion_done(&f->finished));
	KUNIT_EXPECT_TRUE(test, completion_done(&f->shutdown_finished));
	KUNIT_EXPECT_EQ(test, f->ret, -ENODEV);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -ENODEV);
	ws73_frw_expect_untouched(test);
}

static void ws73_frw_transaction_inactive(struct kunit *test)
{
	struct ws73_frw_fixture *f = test->priv;

	ws73_frw_test_reply(f, 1);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transaction_receive(&f->transaction, f->reply,
						     sizeof(f->reply)),
			-EALREADY);
	ws73_frw_transaction_shutdown(&f->transaction);
	KUNIT_EXPECT_EQ(test, ws73_frw_test_call(f, 1000), -ESHUTDOWN);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_transaction_receive(&f->transaction, f->reply,
						     sizeof(f->reply)),
			-ESHUTDOWN);
	KUNIT_EXPECT_EQ(test, f->sends, 0U);
	ws73_frw_expect_untouched(test);
}

static struct kunit_case ws73_frw_transaction_cases[] = {
	KUNIT_CASE(ws73_frw_transaction_success),
	KUNIT_CASE(ws73_frw_transaction_failures),
	KUNIT_CASE(ws73_frw_transaction_deadline),
	KUNIT_CASE(ws73_frw_transaction_timeout),
	KUNIT_CASE(ws73_frw_transaction_exhaustion),
	KUNIT_CASE(ws73_frw_transaction_validation),
	KUNIT_CASE(ws73_frw_transaction_independent_rx),
	KUNIT_CASE(ws73_frw_transaction_shutdown_drain),
	KUNIT_CASE(ws73_frw_transaction_inactive),
	{}
};

static struct kunit_suite ws73_frw_transaction_suite = {
	.name = "ws73-wifi-frw-transaction",
	.init = ws73_frw_fixture_init,
	.test_cases = ws73_frw_transaction_cases,
};

kunit_test_suite(ws73_frw_transaction_suite);

MODULE_LICENSE("GPL");
