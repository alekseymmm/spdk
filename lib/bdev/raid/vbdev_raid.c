/*
 * vbdev_raid.c
 *
 *  Created on: 18 мая 2018 г.
 *      Author: root
 */

#include <stdatomic.h>
#include "spdk/bdev.h"
#include "spdk/log.h"

#include "spdk_internal/bdev.h"

#include "vbdev_common.h"
#include "vbdev_dev.h"
#include "vbdev_raid.h"

static int rdx_raid_create_devices(struct rdx_raid *raid,
				struct rdx_devices *devices);

int spdk_raid_create(char *name, int level, int stripe_size_kb,
		struct rdx_devices *devices, uint64_t raid_size)
{

	g_raid = calloc(1, sizeof(struct rdx_raid));
	if (!g_raid) {
		SPDK_ERRLOG("Cannot allocate memory\n");
		return -ENOMEM;
	}

	g_raid->name = strdup(name);
	g_raid->level = level;
	g_raid->stripe_size_kb = stripe_size_kb;
	g_raid->stripe_size = stripe_size_kb * 1024 / KERNEL_SECTOR_SIZE;
	g_raid->size = raid_size;
	g_raid->dev_cnt = devices->cnt;
//	if (!raid_size)
//		set_bit(RDX_RAID_STATE_CREATE, &raid->state);

	if (rdx_raid_create_devices(g_raid, devices)) {
		SPDK_ERRLOG("Cannot allocate devices\n");
		goto error;
	}
error:
	//rdx_raid_destroy(raid);
	return -ENOMEM;
}

static int rdx_raid_create_devices(struct rdx_raid *raid,
				struct rdx_devices *devices)
{
	int i;

	raid->devices = calloc(raid->dev_cnt, sizeof(struct rdx_dev *));
	if (!raid->devices) {
		SPDK_ERRLOG("Cannot allocate memory\n");
		return -1;
	}

	/* Open drives */
	for (i = 0; i < raid->dev_cnt; i++) {
		rdx_dev_create(raid, devices, i);
	}

//	/* Configure raid and dev sizes or validate after restoring */
//	if (!raid->size) {
//		raid->dev_size = raid->devices[0]->size;
//
//		for (i = 1; i < raid->dev_cnt; i++) {
//			if (raid->devices[i]->size < raid->dev_size)
//				raid->dev_size = raid->devices[i]->size;
//		}
//
//		raid->size = raid->stripe_dsc.data_cnt * raid->dev_size;
//	} else {
//		raid->dev_size = raid->size / raid->stripe_dsc.data_cnt;
//
//		for (i = 0; i < raid->dev_cnt; i++) {
//			if (raid->devices[i]->size &&
//			    raid->devices[i]->size < raid->dev_size) {
//				SPDK_ERRLOG("Device %d is too small for RAID\n", i);
//				return -1;
//			}
//		}
//	}

	/* Update raid->size in metadata on all devices */
//	rdx_md_flush(raid);

	/* Update devices states after restoring to check resulting bitmap */
//	if (!test_bit(RDX_RAID_STATE_CREATE, &raid->state)) {
//		for (i = 0; i < raid->dev_cnt; i++)
//			rdx_dev_update_state(raid->devices[i]);
//	}

	/* Creation of degraded RAID is not allowed */
//	if (test_bit(RDX_RAID_STATE_CREATE, &raid->state) &&
//	    test_bit(RDX_RAID_STATE_DEGRADED, &raid->state)) {
//		SPDK_ERRLOG("Cannot create degraded raid\n");
//		return -1;
//	}

	//clear_bit(RDX_RAID_STATE_CREATE, &raid->state);

	return 0;
}

int rdx_raid_replace(struct rdx_raid *raid, int dev_num, struct spdk_bdev *bdev)
{
	int ret = 0;

	ret = rdx_dev_register(raid->devices[dev_num], bdev);
	if (ret) {
		SPDK_ERRLOG("Failed to replace device %s on position %d\n",
				bdev->name, dev_num);
		return -1;
	}
	SPDK_NOTICELOG("raid %s device=%s replaced on pos %d\n",
			raid->name, bdev->name, dev_num);
	return 0;
}

