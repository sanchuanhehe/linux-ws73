// SPDX-License-Identifier: GPL-2.0-only
#include <crypto/sha2.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/hex.h>
#include <linux/module.h>
#include <linux/string.h>

#include "firmware.h"

/* Original SDK d5e68a58fe38 files, not combined images or firmware bytes. */
static const struct ws73_fw_profile ws73_profiles[] = {
	{
		.package = 0,
		.image = {
			{ "hisilicon/ws73/1.10.110/us/ws73.bin", 143956, 0x400000,
			  "7ac0483476bd0c861125929b034a3b9fe5275e6ef8036d37b370eec3f41d1b23" },
			{ "hisilicon/ws73/1.10.110/us/wifi_cali.bin", 21044, 0x430000,
			  "3d4bfadb1b2a61e1206a68083cbed7b1b109a80cbccab4f4e7825f3a6bce7fc5" },
			{ "hisilicon/ws73/1.10.110/us/btc_cali.bin", 7772, 0x440000,
			  "4010355847dc14d26c471d7be411c27c226ad3628ca2f1855678e6cfc21c5af4" },
		},
	}, {
		.package = 2,
		.image = {
			{ "hisilicon/ws73/1.10.110/e/ws73.bin", 137644, 0x400000,
			  "6a0c322668d67a7e8c5cab9bff5d7af1f737e30ed4d9258975e8068054e4377b" },
			{ "hisilicon/ws73/1.10.110/e/wifi_cali.bin", 21060, 0x430000,
			  "ac55f1a2e1d1466a02123c21e50204e108fe4660c587566e63798de648bd1390" },
			{ "hisilicon/ws73/1.10.110/e/btc_cali.bin", 34120, 0x440000,
			  "601b143db64ef6e70bf882fd5a998ecff59f19ede85f477cdc8967b21a8a472f" },
		},
	},
};

const struct ws73_fw_profile *ws73_fw_profile(const struct btws73_chip_identity *chip)
{
	unsigned int i;

	if (!chip)
		return ERR_PTR(-EINVAL);
	if (chip->bus)
		return ERR_PTR(-EOPNOTSUPP);
	for (i = 0; i < ARRAY_SIZE(ws73_profiles); i++)
		if (chip->package == ws73_profiles[i].package)
			return &ws73_profiles[i];
	return ERR_PTR(-EOPNOTSUPP);
}
EXPORT_SYMBOL_NS_GPL(ws73_fw_profile, "WS73");

int ws73_fw_validate(const struct ws73_fw_info *info, const u8 *data,
		     size_t size, struct ws73_fw_view *view)
{
	u8 digest[SHA256_DIGEST_SIZE];
	char hex[SHA256_DIGEST_SIZE * 2];

	if (!info || !data || !view || !info->sha256)
		return -EINVAL;
	if (size <= WS73_FW_HASH_HEADER || size > WS73_FW_MAX_SIZE || size != info->size)
		return -EMSGSIZE;
	if (size - WS73_FW_HASH_HEADER > U32_MAX - info->address)
		return -EOVERFLOW;
	if (strlen(info->sha256) != sizeof(hex))
		return -EINVAL;

	sha256(data + WS73_FW_HASH_HEADER, size - WS73_FW_HASH_HEADER, digest);
	bin2hex(hex, digest, sizeof(digest));
	if (memcmp(hex, data, sizeof(hex)))
		return -EBADMSG;
	sha256(data, size, digest);
	bin2hex(hex, digest, sizeof(digest));
	if (memcmp(hex, info->sha256, sizeof(hex)))
		return -EKEYREJECTED;

	*view = (struct ws73_fw_view) {
		.data = data + WS73_FW_HASH_HEADER,
		.size = size - WS73_FW_HASH_HEADER,
		.address = info->address,
	};
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_fw_validate, "WS73");

void ws73_firmware_release(struct ws73_firmware *set)
{
	unsigned int i;

	if (!set)
		return;
	for (i = 0; i < WS73_FW_COUNT; i++)
		release_firmware(set->file[i]);
	memset(set, 0, sizeof(*set));
}

int ws73_firmware_request(struct device *dev, const struct btws73_chip_identity *chip,
			  struct ws73_firmware *set)
{
	struct ws73_firmware loaded = {};
	unsigned int i;
	int ret;

	if (!dev || !set)
		return -EINVAL;
	/* Caller supplies an empty, exclusively owned bundle. */
	if (set->profile || memchr_inv(set->file, 0, sizeof(set->file)))
		return -EBUSY;
	loaded.profile = ws73_fw_profile(chip);
	if (IS_ERR(loaded.profile))
		return PTR_ERR(loaded.profile);
	for (i = 0; i < WS73_FW_COUNT; i++) {
		const struct ws73_fw_info *info = &loaded.profile->image[i];

		/* No userspace fallback wait during a cancellable boot attempt. */
		ret = request_firmware_direct(&loaded.file[i], info->name, dev);
		if (ret)
			goto fail;
		ret = ws73_fw_validate(info, loaded.file[i]->data, loaded.file[i]->size,
				       &loaded.image[i]);
		if (ret)
			goto fail;
	}
	/* Publish only when every required image has passed. No device I/O. */
	*set = loaded;
	return 0;
fail:
	ws73_firmware_release(&loaded);
	return ret;
}

MODULE_FIRMWARE("hisilicon/ws73/1.10.110/us/ws73.bin");
MODULE_FIRMWARE("hisilicon/ws73/1.10.110/us/wifi_cali.bin");
MODULE_FIRMWARE("hisilicon/ws73/1.10.110/us/btc_cali.bin");
MODULE_FIRMWARE("hisilicon/ws73/1.10.110/e/ws73.bin");
MODULE_FIRMWARE("hisilicon/ws73/1.10.110/e/wifi_cali.bin");
MODULE_FIRMWARE("hisilicon/ws73/1.10.110/e/btc_cali.bin");
