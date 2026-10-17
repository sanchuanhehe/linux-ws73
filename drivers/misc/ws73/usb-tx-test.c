// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/unaligned.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/sched/task.h>

#include "usb-tx.h"

struct ws73_tx_mock {
	struct kunit *test;
	u64 now;
	u64 deadline;
	u32 sequence;
	unsigned int calls;
	unsigned int fail_at;
	unsigned int fault;
	u8 queue;
	u8 step;
	bool cancelled;
};

static u64 ws73_tx_now(void *context)
{
	return ((struct ws73_tx_mock *)context)->now;
}

static bool ws73_tx_cancelled(void *context)
{
	return ((struct ws73_tx_mock *)context)->cancelled;
}

static int ws73_tx_send(void *context, const u8 *data, size_t length, unsigned int timeout)
{
	struct ws73_tx_mock *mock = context;
	struct kunit *test = mock->test;

	KUNIT_EXPECT_LE(test, timeout, 2000);
	KUNIT_EXPECT_GT(test, timeout, 0);
	KUNIT_EXPECT_LE(test, (u64)timeout, mock->deadline - mock->now);
	KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 8), mock->sequence++);
	if (mock->step < 2) {
		KUNIT_EXPECT_EQ(test, length, 36);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data), 3);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 4), 0);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 12), 1 - mock->step);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 16),
				mock->step ? 0 : mock->queue);
		KUNIT_EXPECT_PTR_EQ(test, memchr_inv(data + 20, 0, 16), NULL);
		mock->step++;
	} else {
		KUNIT_EXPECT_EQ(test, length, 128);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data), 0);
		KUNIT_EXPECT_EQ(test, get_unaligned_le32(data + 4), 128);
		KUNIT_EXPECT_EQ(test, data[65], mock->queue);
	}
	mock->calls++;
	mock->now += 3;
	if (mock->calls != mock->fail_at)
		return length;
	switch (mock->fault) {
	case 0:
		return -EPIPE;
	case 1:
		return length - 1;
	case 2:
		return length + 1;
	case 3:
		mock->now = mock->deadline;
		break;
	case 4:
		mock->cancelled = true;
		break;
	}
	return length;
}

static struct ws73_usb_tx_io ws73_tx_io(struct ws73_tx_mock *mock)
{
	return (struct ws73_usb_tx_io) {
		.context = mock,
		.now_ms = ws73_tx_now,
		.cancelled = ws73_tx_cancelled,
		.send = ws73_tx_send,
	};
}

static const u8 ws73_tx_ble[] = { 0, 7, 4, 0, 1, 3, 0x0c, 0 };

static void ws73_usb_tx_sequence_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_usb_tx *other = kunit_kzalloc(test, sizeof(*other), GFP_KERNEL);
	struct ws73_tx_mock mock = { .test = test, .deadline = 100, .queue = 9 };
	struct ws73_usb_tx_io io = ws73_tx_io(&mock);
	struct ws73_tx_mock peer = mock;
	struct ws73_usb_tx_io peer_io = ws73_tx_io(&peer);
	u8 platform[32];

	KUNIT_ASSERT_NOT_NULL(test, tx);
	KUNIT_ASSERT_NOT_NULL(test, other);
	ws73_usb_tx_init(tx);
	ws73_usb_tx_init(other);
	KUNIT_ASSERT_EQ(test, btws73_chip_query_encode(platform, 32), 32);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, platform, 32, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	KUNIT_EXPECT_TRUE(test, tx->queue_valid);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, platform, 32, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 4);
	mock.queue = 7;
	mock.step = 0;
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 7);
	KUNIT_EXPECT_EQ(test, tx->queue, 7);
	/* A second instance still needs its own switch/reset and sequence zero. */
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(other, &peer_io, platform, 32, 100), 0);
	KUNIT_EXPECT_EQ(test, peer.calls, 3);
	KUNIT_EXPECT_EQ(test, tx->sequence, 7);
	/* USB counter wraps; no reply matcher depends on this value. */
	tx->sequence = U32_MAX;
	mock.sequence = U32_MAX;
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
	KUNIT_EXPECT_EQ(test, tx->sequence, 0);
	KUNIT_ASSERT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
}

static void ws73_usb_tx_faults_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_tx_mock mock;
	struct ws73_usb_tx_io io = ws73_tx_io(&mock);
	unsigned int step, fault;
	int expected;

	KUNIT_ASSERT_NOT_NULL(test, tx);
	for (step = 1; step <= 3; step++) {
		for (fault = 0; fault < 5; fault++) {
			/* Fresh isolated synthetic session; no live callbacks exist. */
			ws73_usb_tx_init(tx);
			mock = (struct ws73_tx_mock) {
				.test = test, .deadline = 100, .queue = 7,
				.fail_at = step, .fault = fault,
			};
			expected = !fault ? -EPIPE : fault < 3 ? -EREMOTEIO :
				fault == 3 ? -ETIMEDOUT : -ENODEV;
			KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100),
					expected);
			KUNIT_EXPECT_EQ(test, mock.calls, step);
			KUNIT_EXPECT_FALSE(test, tx->queue_valid);
			/* Clearing external fault/deadline cannot resurrect this instance. */
			mock.cancelled = false;
			mock.now = 0;
			mock.fail_at = 0;
			KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100),
					expected);
			KUNIT_EXPECT_EQ(test, mock.calls, step);
		}
	}
}

static void ws73_usb_tx_admission_test(struct kunit *test)
{
	struct ws73_usb_tx *tx = kunit_kzalloc(test, sizeof(*tx), GFP_KERNEL);
	struct ws73_tx_mock mock = { .test = test, .deadline = 100, .queue = 7 };
	struct ws73_usb_tx_io io = ws73_tx_io(&mock);
	u8 invalid[8] = { 0, 8, 4 };

	KUNIT_ASSERT_NOT_NULL(test, tx);
	ws73_usb_tx_init(tx);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, invalid, 8, 100), -EOPNOTSUPP);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 3, 100), -EMSGSIZE);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, NULL, 8, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(NULL, &io, ws73_tx_ble, 8, 100), -EINVAL);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, NULL, ws73_tx_ble, 8, 100), -EINVAL);
	io.send = NULL;
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), -EINVAL);
	io.send = ws73_tx_send;
	KUNIT_EXPECT_EQ(test, mock.calls, 0);
	KUNIT_EXPECT_EQ(test, tx->failure, 0);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), 0);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	/* Failed admission cannot revoke another function's transport. */
	ws73_usb_tx_init(tx);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, mock.now),
			-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	KUNIT_EXPECT_EQ(test, tx->failure, 0);
	ws73_usb_tx_init(tx);
	mock.cancelled = true;
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(tx, &io, ws73_tx_ble, 8, 100), -ENODEV);
	KUNIT_EXPECT_EQ(test, mock.calls, 3);
	KUNIT_EXPECT_EQ(test, tx->failure, 0);
}

struct ws73_tx_contender {
	struct ws73_tx_contention *fixture;
	struct ws73_usb_tx_io io;
	struct completion entered, done;
	wait_queue_head_t stop;
	bool owner;
	unsigned int calls;
	int result;
};

struct ws73_tx_contention {
	struct kunit *test;
	struct ws73_usb_tx tx;
	struct ws73_tx_contender owner, waiter;
	struct completion release;
	bool fail_owner;
};

static u64 ws73_tx_clock(void *context)
{
	return ktime_to_ms(ktime_get_boottime());
}

static bool ws73_tx_not_cancelled(void *context)
{
	return false;
}

static int ws73_tx_contended_send(void *context, const u8 *data, size_t length,
				  unsigned int timeout)
{
	struct ws73_tx_contender *caller = context;
	struct ws73_tx_contention *f = caller->fixture;
	u8 before[128];

	caller->calls++;
	if (length > sizeof(before))
		return -EMSGSIZE;
	memcpy(before, data, length);
	if (caller->owner && caller->calls == 1) {
		complete(&caller->entered);
		if (!wait_for_completion_timeout(&f->release, msecs_to_jiffies(1500)))
			return -ETIMEDOUT;
		KUNIT_EXPECT_MEMEQ(f->test, before, data, length);
		if (f->fail_owner)
			return -EPIPE;
	}
	return length;
}

static int ws73_tx_contender_thread(void *context)
{
	struct ws73_tx_contender *caller = context;

	caller->result = ws73_usb_tx_record(&caller->fixture->tx, &caller->io,
					    ws73_tx_ble, sizeof(ws73_tx_ble),
					    ws73_tx_clock(NULL) + 2000);
	complete(&caller->done);
	wait_event_interruptible(caller->stop, kthread_should_stop());
	return 0;
}

static void ws73_tx_stop_contender(void *context)
{
	struct task_struct *task = context;
	struct ws73_tx_contender *caller = kthread_data(task);

	complete_all(&caller->fixture->release);
	kthread_stop(task);
	put_task_struct(task);
}

static void ws73_tx_start_contender(struct kunit *test, struct ws73_tx_contender *caller)
{
	struct task_struct *task;

	task = kthread_create(ws73_tx_contender_thread, caller, "ws73-tx-wait");
	KUNIT_ASSERT_FALSE(test, IS_ERR(task));
	get_task_struct(task);
	KUNIT_ASSERT_EQ(test, kunit_add_action_or_reset(test, ws73_tx_stop_contender, task), 0);
	wake_up_process(task);
}

static struct ws73_tx_contention *ws73_tx_contention_init(struct kunit *test)
{
	struct ws73_tx_contention *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	struct ws73_tx_contender *callers[2];
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, f);
	f->test = test;
	ws73_usb_tx_init(&f->tx);
	init_completion(&f->release);
	callers[0] = &f->owner;
	callers[1] = &f->waiter;
	for (i = 0; i < ARRAY_SIZE(callers); i++) {
		struct ws73_tx_contender *caller = callers[i];

		caller->fixture = f;
		caller->owner = !i;
		caller->io = (struct ws73_usb_tx_io) {
			.context = caller, .now_ms = ws73_tx_clock,
			.cancelled = ws73_tx_not_cancelled, .send = ws73_tx_contended_send,
		};
		init_completion(&caller->entered);
		init_completion(&caller->done);
		init_waitqueue_head(&caller->stop);
	}
	return f;
}

static void ws73_usb_tx_contention_timeout_test(struct kunit *test)
{
	struct ws73_tx_contention *f = ws73_tx_contention_init(test);
	u64 start;

	ws73_tx_start_contender(test, &f->owner);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->owner.entered, HZ), 0UL);
	start = ws73_tx_clock(NULL);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(&f->tx, &f->waiter.io, ws73_tx_ble,
						 sizeof(ws73_tx_ble), start + 20), -ETIMEDOUT);
	/* The owner is still blocked. Admission cannot overwrite its buffer,
	 * consume a USB sequence or poison its eventual successful completion.
	 */
	KUNIT_EXPECT_FALSE(test, completion_done(&f->owner.done));
	KUNIT_EXPECT_LT(test, ws73_tx_clock(NULL) - start, 500ULL);
	KUNIT_EXPECT_EQ(test, f->waiter.calls, 0);
	KUNIT_EXPECT_EQ(test, READ_ONCE(f->tx.failure), 0);
	KUNIT_EXPECT_EQ(test, READ_ONCE(f->tx.sequence), 1);
	complete_all(&f->release);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->owner.done, HZ), 0UL);
	KUNIT_EXPECT_EQ(test, f->owner.result, 0);
	KUNIT_EXPECT_EQ(test, f->owner.calls, 3);
	KUNIT_EXPECT_EQ(test, ws73_usb_tx_record(&f->tx, &f->waiter.io, ws73_tx_ble,
						 sizeof(ws73_tx_ble),
						 ws73_tx_clock(NULL) + 1000), 0);
	KUNIT_EXPECT_EQ(test, f->waiter.calls, 1);
}

static void ws73_usb_tx_contention_handoff_test(struct kunit *test)
{
	unsigned int fault;

	for (fault = 0; fault < 2; fault++) {
		struct ws73_tx_contention *f = ws73_tx_contention_init(test);
		unsigned long limit;
		bool waiting = false;

		f->fail_owner = fault;
		ws73_tx_start_contender(test, &f->owner);
		KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->owner.entered, HZ), 0UL);
		ws73_tx_start_contender(test, &f->waiter);
		limit = jiffies + HZ;
		do {
			spin_lock_irq(&f->tx.available.lock);
			/* Queue lock protects observation, not wake suppression. */
			waiting = waitqueue_active(&f->tx.available);
			spin_unlock_irq(&f->tx.available.lock);
			if (waiting)
				break;
			usleep_range(1000, 2000);
		} while (time_before(jiffies, limit));
		KUNIT_EXPECT_TRUE(test, waiting);
		KUNIT_EXPECT_FALSE(test, completion_done(&f->waiter.done));
		complete_all(&f->release);
		KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->owner.done, HZ), 0UL);
		KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->waiter.done, HZ), 0UL);
		KUNIT_EXPECT_EQ(test, f->owner.result, fault ? -EPIPE : 0);
		KUNIT_EXPECT_EQ(test, f->waiter.result, fault ? -EPIPE : 0);
		KUNIT_EXPECT_EQ(test, f->owner.calls, fault ? 1 : 3);
		KUNIT_EXPECT_EQ(test, f->waiter.calls, fault ? 0 : 1);
	}
}

static struct kunit_case ws73_usb_tx_cases[] = {
	KUNIT_CASE(ws73_usb_tx_sequence_test),
	KUNIT_CASE(ws73_usb_tx_faults_test),
	KUNIT_CASE(ws73_usb_tx_admission_test),
	KUNIT_CASE(ws73_usb_tx_contention_timeout_test),
	KUNIT_CASE(ws73_usb_tx_contention_handoff_test),
	{}
};

static struct kunit_suite ws73_usb_tx_suite = {
	.name = "ws73-usb-tx",
	.test_cases = ws73_usb_tx_cases,
};

kunit_test_suite(ws73_usb_tx_suite);
