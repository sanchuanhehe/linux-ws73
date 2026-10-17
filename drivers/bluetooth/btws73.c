// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/ws73-ble.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "btws73_proto.h"

#define BTWS73_TX_PACKETS 64
#define BTWS73_TX_BYTES (64 * 1024)

struct btws73 {
	struct auxiliary_device *adev;
	struct hci_dev *hdev;
	struct mutex registration; /* HCI registration versus deferred faults. */
	struct mutex lifecycle; /* Serialize open, close and queue flush. */
	spinlock_t lock; /* Queue admission/accounting and H4 receive state. */
	struct work_struct tx_work;
	struct work_struct fault_work;
	struct sk_buff_head tx;
	unsigned int packets;
	unsigned int bytes;
	bool running;
	bool flushing;
	bool registered;
	int error;
	struct btws73_h4 rx;
	u8 record[1504]; /* Serialized TX worker storage, never URB memory. */
};

static void btws73_fault_work(struct work_struct *work)
{
	struct btws73 *bt = container_of(work, struct btws73, fault_work);
	unsigned long flags;
	int error;

	/* Removal can replace the terminal error while this work is running. */
	spin_lock_irqsave(&bt->lock, flags);
	error = bt->error;
	spin_unlock_irqrestore(&bt->lock, flags);

	/* RX parse failures become terminal without reentering its callback. */
	ws73_ble_fault(bt->adev, error);
	/* No fabricated HCI Hardware Error or command-complete event. */
	mutex_lock(&bt->registration);
	if (bt->registered) {
		hci_cmd_sync_cancel_sync(bt->hdev, -error);
		hci_unregister_dev(bt->hdev);
		bt->registered = false;
	}
	mutex_unlock(&bt->registration);
}

static void btws73_fault_locked(struct btws73 *bt, int error)
{
	lockdep_assert_held(&bt->lock);
	if (!bt->error) {
		bt->error = error;
		bt->running = false;
		schedule_work(&bt->fault_work);
	}
}

static void btws73_fault(void *context, int error)
{
	struct btws73 *bt = context;
	unsigned long flags;

	spin_lock_irqsave(&bt->lock, flags);
	btws73_fault_locked(bt, error);
	spin_unlock_irqrestore(&bt->lock, flags);
}

static void btws73_tx_work(struct work_struct *work)
{
	struct btws73 *bt = container_of(work, struct btws73, tx_work);
	struct sk_buff *skb;
	unsigned long flags;
	int ret;

	for (;;) {
		spin_lock_irqsave(&bt->lock, flags);
		skb = bt->running && !bt->flushing ? __skb_dequeue(&bt->tx) : NULL;
		spin_unlock_irqrestore(&bt->lock, flags);
		if (!skb)
			return;
		ret = btws73_hcc_encode(bt->record, sizeof(bt->record), skb->data, skb->len);
		if (ret > 0)
			ret = ws73_ble_send(bt->adev, bt->record, ret);
		if (!ret) {
			bt->hdev->stat.byte_tx += skb->len - 1;
			if (skb->data[0] == HCI_COMMAND_PKT)
				bt->hdev->stat.cmd_tx++;
			else
				bt->hdev->stat.acl_tx++;
		} else {
			bt->hdev->stat.err_tx++;
		}
		spin_lock_irqsave(&bt->lock, flags);
		bt->packets--;
		bt->bytes -= skb->len;
		spin_unlock_irqrestore(&bt->lock, flags);
		kfree_skb(skb);
		if (ret) {
			ws73_ble_fault(bt->adev, ret);
			return;
		}
	}
}

static int btws73_flush_locked(struct hci_dev *hdev)
{
	struct btws73 *bt = hci_get_drvdata(hdev);
	struct sk_buff_head discard;
	unsigned long flags;
	int ret = 0;

	skb_queue_head_init(&discard);
	spin_lock_irqsave(&bt->lock, flags);
	bt->flushing = true;
	spin_unlock_irqrestore(&bt->lock, flags);
	cancel_work_sync(&bt->tx_work);
	spin_lock_irqsave(&bt->lock, flags);
	/* Purging accepted ACL in a live session would strand native credits. */
	if (bt->running && !skb_queue_empty(&bt->tx)) {
		ret = -ECANCELED;
		btws73_fault_locked(bt, ret);
	}
	skb_queue_splice_init(&bt->tx, &discard);
	bt->packets = 0;
	bt->bytes = 0;
	bt->flushing = false;
	spin_unlock_irqrestore(&bt->lock, flags);
	skb_queue_purge(&discard);
	return ret;
}

static int btws73_flush(struct hci_dev *hdev)
{
	struct btws73 *bt = hci_get_drvdata(hdev);
	int ret;

	mutex_lock(&bt->lifecycle);
	ret = btws73_flush_locked(hdev);
	mutex_unlock(&bt->lifecycle);
	return ret;
}

static int btws73_open(struct hci_dev *hdev)
{
	struct btws73 *bt = hci_get_drvdata(hdev);
	unsigned long flags;
	int ret;

	mutex_lock(&bt->lifecycle);
	spin_lock_irqsave(&bt->lock, flags);
	ret = bt->error;
	if (!ret) {
		btws73_h4_reset(&bt->rx);
		bt->running = true;
	}
	spin_unlock_irqrestore(&bt->lock, flags);
	if (!ret) {
		ret = ws73_ble_start(bt->adev);
		if (ret)
			btws73_fault(bt, ret);
	}
	mutex_unlock(&bt->lifecycle);
	return ret;
}

static int btws73_close(struct hci_dev *hdev)
{
	struct btws73 *bt = hci_get_drvdata(hdev);
	unsigned long flags;
	int ret;

	mutex_lock(&bt->lifecycle);
	spin_lock_irqsave(&bt->lock, flags);
	bt->running = false;
	spin_unlock_irqrestore(&bt->lock, flags);
	btws73_flush_locked(hdev);
	ret = ws73_ble_stop(bt->adev);
	mutex_unlock(&bt->lifecycle);
	return ret == -EALREADY ? 0 : ret;
}

static int btws73_setup(struct hci_dev *hdev)
{
	struct hci_rp_read_local_features *features;
	struct hci_rp_read_bd_addr *address;
	struct sk_buff *skb;
	int ret = 0;

	/* Native HCI requests: only actual controller events can complete these. */
	skb = __hci_cmd_sync(hdev, HCI_OP_READ_LOCAL_FEATURES, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb))
		return PTR_ERR(skb);
	features = (void *)skb->data;
	if (skb->len != sizeof(*features) || features->status)
		ret = -EIO;
	else if (!(features->features[4] & LMP_LE) ||
		 !(features->features[4] & LMP_NO_BREDR))
		ret = -EOPNOTSUPP;
	kfree_skb(skb);
	if (ret)
		return ret;
	skb = __hci_cmd_sync(hdev, HCI_OP_READ_BD_ADDR, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb))
		return PTR_ERR(skb);
	address = (void *)skb->data;
	if (skb->len != sizeof(*address) || address->status)
		ret = -EIO;
	else if (!bacmp(&address->bdaddr, BDADDR_ANY) ||
		 !bacmp(&address->bdaddr, BDADDR_NONE))
		ret = -EADDRNOTAVAIL;
	else
		bacpy(&hdev->bdaddr, &address->bdaddr);
	kfree_skb(skb);
	return ret;
}

static int btws73_send(struct hci_dev *hdev, struct sk_buff *skb)
{
	struct btws73 *bt = hci_get_drvdata(hdev);
	struct sk_buff *copy;
	unsigned long flags;
	unsigned int length;
	int ret;

	spin_lock_irqsave(&bt->lock, flags);
	ret = bt->error ?: (bt->running ? 0 : -ENETDOWN);
	if (ret)
		goto unlock;
	if (bt->flushing) {
		ret = -ECANCELED;
		goto reject;
	}
	if (!skb->len || skb->len >= BTWS73_H4_MAX_SIZE) {
		ret = -EMSGSIZE;
		goto reject;
	}
	if (hci_skb_pkt_type(skb) != HCI_COMMAND_PKT &&
	    hci_skb_pkt_type(skb) != HCI_ACLDATA_PKT) {
		ret = -EOPNOTSUPP;
		goto reject;
	}
	length = skb->len + 1;
	if (bt->packets >= BTWS73_TX_PACKETS || length > BTWS73_TX_BYTES - bt->bytes) {
		ret = -ENOBUFS;
		goto reject;
	}
	copy = skb_copy_expand(skb, 1, 0, GFP_ATOMIC);
	if (!copy) {
		ret = -ENOMEM;
		goto reject;
	}
	*(u8 *)skb_push(copy, 1) = hci_skb_pkt_type(skb);
	ret = btws73_hci_validate(copy->data, copy->len, true);
	if (ret) {
		kfree_skb(copy);
		goto reject;
	}
	__skb_queue_tail(&bt->tx, copy);
	bt->packets++;
	bt->bytes += copy->len;
	schedule_work(&bt->tx_work);
	goto unlock;
reject:
	/* hci_sched_acl_pkt consumes credit even when ->send returns an error.
	 * Close admission before unlocking; do not invent a completion/credit.
	 */
	bt->hdev->stat.err_tx++;
	btws73_fault_locked(bt, ret);
unlock:
	spin_unlock_irqrestore(&bt->lock, flags);
	/* An error leaves the original skb with the HCI caller. */
	if (!ret)
		kfree_skb(skb);
	return ret;
}

static void btws73_receive(void *context, const u8 *data, size_t length)
{
	struct btws73 *bt = context;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&bt->lock, flags);
	if (!bt->running)
		goto unlock;
	while (length) {
		const u8 *packet;
		size_t used, size;
		struct sk_buff *skb;

		ret = btws73_h4_feed(&bt->rx, data, length, &used, &packet, &size);
		if (ret < 0)
			break;
		data += used;
		length -= used;
		if (!ret)
			break;
		skb = bt_skb_alloc(size - 1, GFP_ATOMIC);
		if (!skb) {
			ret = -ENOMEM;
			break;
		}
		hci_skb_pkt_type(skb) = packet[0];
		skb_put_data(skb, packet + 1, size - 1);
		bt->hdev->stat.byte_rx += size - 1;
		ret = hci_recv_frame(bt->hdev, skb);
		if (ret)
			break;
	}
	if (ret < 0) {
		bt->hdev->stat.err_rx++;
		/* Close TX admission before dropping the receive-state lock.
		 * Only the worker may reenter the shared fault interface.
		 */
		btws73_fault_locked(bt, ret);
	}
unlock:
	spin_unlock_irqrestore(&bt->lock, flags);
}

static const struct ws73_ble_client btws73_client = {
	.receive = btws73_receive,
	.fault = btws73_fault,
};

static int btws73_probe(struct auxiliary_device *adev,
			const struct auxiliary_device_id *id)
{
	struct btws73 *bt;
	int ret;

	bt = kzalloc_obj(*bt);
	if (!bt)
		return -ENOMEM;
	bt->hdev = hci_alloc_dev();
	if (!bt->hdev) {
		kfree(bt);
		return -ENOMEM;
	}
	bt->adev = adev;
	mutex_init(&bt->registration);
	mutex_init(&bt->lifecycle);
	spin_lock_init(&bt->lock);
	skb_queue_head_init(&bt->tx);
	INIT_WORK(&bt->tx_work, btws73_tx_work);
	INIT_WORK(&bt->fault_work, btws73_fault_work);
	bt->hdev->bus = HCI_USB;
	SET_HCIDEV_DEV(bt->hdev, &adev->dev);
	hci_set_drvdata(bt->hdev, bt);
	bt->hdev->setup = btws73_setup;
	bt->hdev->open = btws73_open;
	bt->hdev->close = btws73_close;
	bt->hdev->flush = btws73_flush;
	bt->hdev->send = btws73_send;
	auxiliary_set_drvdata(adev, bt);
	mutex_lock(&bt->registration);
	ret = ws73_ble_bind(adev, &btws73_client, bt);
	if (ret) {
		mutex_unlock(&bt->registration);
		goto free;
	}
	ret = hci_register_dev(bt->hdev);
	if (!ret)
		bt->registered = true;
	mutex_unlock(&bt->registration);
	if (!ret)
		return 0;
	ws73_ble_unbind(adev);
	cancel_work_sync(&bt->fault_work);
free:
	auxiliary_set_drvdata(adev, NULL);
	hci_free_dev(bt->hdev);
	kfree(bt);
	return ret;
}

static void btws73_remove(struct auxiliary_device *adev)
{
	struct btws73 *bt = auxiliary_get_drvdata(adev);
	unsigned long flags;
	int error;

	ws73_ble_unbind(adev);
	spin_lock_irqsave(&bt->lock, flags);
	bt->running = false;
	error = bt->error;
	bt->error = -ENODEV;
	spin_unlock_irqrestore(&bt->lock, flags);
	if (error)
		ws73_ble_fault(adev, error);
	btws73_flush(bt->hdev);
	cancel_work_sync(&bt->fault_work);
	if (bt->registered) {
		hci_cmd_sync_cancel_sync(bt->hdev, ENODEV);
		hci_unregister_dev(bt->hdev);
	}
	auxiliary_set_drvdata(adev, NULL);
	hci_free_dev(bt->hdev);
	kfree(bt);
}

static const struct auxiliary_device_id btws73_ids[] = {
	{ .name = "ws73_core.ble" },
	{}
};
MODULE_DEVICE_TABLE(auxiliary, btws73_ids);

static struct auxiliary_driver btws73_driver = {
	.name = "btws73",
	.probe = btws73_probe,
	.remove = btws73_remove,
	.id_table = btws73_ids,
};
module_auxiliary_driver(btws73_driver);

MODULE_DESCRIPTION("WS73 USB native Bluetooth LE HCI consumer");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
MODULE_IMPORT_NS("BTWS73");
