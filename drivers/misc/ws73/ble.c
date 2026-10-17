// SPDX-License-Identifier: GPL-2.0-only
#include <linux/align.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/unaligned.h>

#include "ble.h"

struct ws73_ble {
	struct auxiliary_device adev;
	struct mutex operation; /* Serialize sleepable backend calls and removal. */
	spinlock_t lock; /* Admission, state and atomic client callbacks. */
	const struct ws73_ble_backend *backend;
	void *backend_context;
	const struct ws73_ble_client *client;
	void *client_context;
	int error;
	bool enabled;
};

static DEFINE_IDA(ws73_ble_ids);

static struct ws73_ble *ws73_ble_from_aux(struct auxiliary_device *adev)
{
	return container_of(adev, struct ws73_ble, adev);
}

int ws73_ble_bind(struct auxiliary_device *adev, const struct ws73_ble_client *client,
		  void *context)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);
	unsigned long flags;
	int ret;

	if (!client || !client->receive || !client->fault)
		return -EINVAL;
	spin_lock_irqsave(&ble->lock, flags);
	ret = ble->error ?: ((ble->client || ble->enabled) ? -EBUSY : 0);
	if (!ret) {
		ble->client = client;
		ble->client_context = context;
	}
	spin_unlock_irqrestore(&ble->lock, flags);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_bind, "WS73");

void ws73_ble_unbind(struct auxiliary_device *adev)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);
	unsigned long flags;

	spin_lock_irqsave(&ble->lock, flags);
	ble->client = NULL;
	ble->client_context = NULL;
	spin_unlock_irqrestore(&ble->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_unbind, "WS73");

void ws73_ble_fault(struct auxiliary_device *adev, int error)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);
	unsigned long flags;

	if (WARN_ON_ONCE(error >= 0))
		return;
	spin_lock_irqsave(&ble->lock, flags);
	if (!ble->error) {
		ble->error = error;
		ble->enabled = false;
		if (ble->client)
			ble->client->fault(ble->client_context, error);
	}
	spin_unlock_irqrestore(&ble->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_fault, "WS73");

static int ws73_ble_set(struct auxiliary_device *adev, bool enabled)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);
	unsigned long flags;
	int ret;

	mutex_lock(&ble->operation);
	spin_lock_irqsave(&ble->lock, flags);
	ret = ble->error;
	if (!ret && enabled && !ble->client)
		ret = -ENOTCONN;
	if (!ret && ble->enabled == enabled)
		ret = -EALREADY;
	/* Gate and drain RX before the disable transaction. */
	if (!enabled)
		ble->enabled = false;
	spin_unlock_irqrestore(&ble->lock, flags);
	if (ret)
		goto unlock;
	ret = ble->backend->set_enabled(ble->backend_context, enabled);
	if (ret) {
		ws73_ble_fault(adev, ret);
	} else {
		spin_lock_irqsave(&ble->lock, flags);
		ret = ble->error;
		if (!ret)
			ble->enabled = enabled;
		spin_unlock_irqrestore(&ble->lock, flags);
	}
unlock:
	mutex_unlock(&ble->operation);
	return ret;
}

int ws73_ble_start(struct auxiliary_device *adev)
{
	return ws73_ble_set(adev, true);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_start, "WS73");

int ws73_ble_stop(struct auxiliary_device *adev)
{
	return ws73_ble_set(adev, false);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_stop, "WS73");

int ws73_ble_send(struct auxiliary_device *adev, const u8 *record, size_t length)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);
	unsigned long flags;
	int ret;

	/* Only an aligned, bounded BLE HCC record belongs to this function. */
	if (!record || length < 8 || length > 1504 || length % 8 || record[0] ||
	    record[1] != 7 || !get_unaligned_le16(record + 2) ||
	    ALIGN(4 + get_unaligned_le16(record + 2), 8) != length ||
	    memchr_inv(record + 4 + get_unaligned_le16(record + 2), 0,
		       length - 4 - get_unaligned_le16(record + 2)))
		return -EINVAL;
	mutex_lock(&ble->operation);
	spin_lock_irqsave(&ble->lock, flags);
	ret = ble->error ?: (ble->enabled ? 0 : -ENETDOWN);
	spin_unlock_irqrestore(&ble->lock, flags);
	if (!ret) {
		ret = ble->backend->send(ble->backend_context, record, length);
		if (ret)
			ws73_ble_fault(adev, ret);
	}
	mutex_unlock(&ble->operation);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_send, "WS73");

void ws73_ble_receive(struct auxiliary_device *adev, const u8 *data, size_t length)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);
	unsigned long flags;

	spin_lock_irqsave(&ble->lock, flags);
	if (!ble->error && ble->enabled && ble->client)
		ble->client->receive(ble->client_context, data, length);
	spin_unlock_irqrestore(&ble->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_receive, "WS73");

static void ws73_ble_release(struct device *dev)
{
	struct ws73_ble *ble = ws73_ble_from_aux(to_auxiliary_dev(dev));

	ida_free(&ws73_ble_ids, ble->adev.id);
	kfree(ble);
	module_put(THIS_MODULE);
}

struct auxiliary_device *ws73_ble_prepare(struct device *parent,
					  const struct ws73_ble_backend *backend, void *context)
{
	struct ws73_ble *ble;
	int ret;

	if (!parent || !backend || !backend->set_enabled || !backend->send)
		return ERR_PTR(-EINVAL);
	ble = kzalloc_obj(*ble);
	if (!ble)
		return ERR_PTR(-ENOMEM);
	ret = ida_alloc(&ws73_ble_ids, GFP_KERNEL);
	if (ret < 0) {
		kfree(ble);
		return ERR_PTR(ret);
	}
	__module_get(THIS_MODULE);
	ble->adev.id = ret;
	ble->adev.name = "ble";
	ble->adev.dev.parent = parent;
	ble->adev.dev.release = ws73_ble_release;
	ble->backend = backend;
	ble->backend_context = context;
	mutex_init(&ble->operation);
	spin_lock_init(&ble->lock);
	ret = auxiliary_device_init(&ble->adev);
	if (ret) {
		ws73_ble_release(&ble->adev.dev);
		return ERR_PTR(ret);
	}
	return &ble->adev;
}

/* Unpublished allocation: no auxiliary_device_delete() is needed. */
void ws73_ble_discard(struct auxiliary_device *adev)
{
	auxiliary_device_uninit(adev);
}

struct auxiliary_device *ws73_ble_add(struct device *parent,
				      const struct ws73_ble_backend *backend, void *context)
{
	struct auxiliary_device *adev;
	int ret;

	adev = ws73_ble_prepare(parent, backend, context);
	if (IS_ERR(adev))
		return adev;
	ret = auxiliary_device_add(adev);
	if (ret) {
		ws73_ble_discard(adev);
		return ERR_PTR(ret);
	}
	return adev;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_add, "WS73");

void ws73_ble_remove(struct auxiliary_device *adev)
{
	struct ws73_ble *ble = ws73_ble_from_aux(adev);

	ws73_ble_fault(adev, -ENODEV);
	mutex_lock(&ble->operation);
	ble->backend = NULL;
	ble->backend_context = NULL;
	mutex_unlock(&ble->operation);
	auxiliary_device_delete(adev);
	ws73_ble_unbind(adev);
	auxiliary_device_uninit(adev);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_remove, "WS73");
