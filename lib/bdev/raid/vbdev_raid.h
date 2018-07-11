/*
 * vbdev_raid.h
 *
 *  Created on: 10 мая 2018 г.
 *      Author: alekseym
 */

#ifndef LIB_BDEV_RAID_VBDEV_RAID_H_
#define LIB_BDEV_RAID_VBDEV_RAID_H_

#include "spdk/stdinc.h"
#include "spdk/bdev.h"

#include "vbdev_common.h"

int spdk_raid_create(char *name, int level, int stripe_size_kb,
		struct rdx_devices *devices, uint64_t raid_size);
int rdx_raid_register(struct rdx_raid *raid);
int rdx_raid_replace(struct rdx_raid_dsc *raid_dsc, int dev_num,
		struct spdk_bdev *bdev);

int create_raid_disk(const char *bdev_name, const char *vbdev_name);

#endif /* LIB_BDEV_RAID_VBDEV_RAID_H_ */
