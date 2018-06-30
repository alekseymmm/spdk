/*
 * vbdev_raid_dsc.c
 *
 *  Created on: Jun 30, 2018
 *      Author: root
 */
#include "vbdev_common.h"
#include "vbdev_raid_dsc.h"
#include "vbdev_dev.h"

static int rdx_dsc_configure(struct rdx_raid_dsc *raid_dsc)
{
	switch (raid_dsc->level) {
	case 0:
		return rdx_raid0_dsc_configure(raid_dsc);
	case 1:
		return -1;//rdx_raid1_dsc_configure(raid_dsc);
	case 5:
		return -1;//rdx_raid6_dsc_configure(raid_dsc, 1);
	case 6:
		return -1;//rdx_raid6_dsc_configure(raid_dsc, 2);
	case 7:
		return -1;//rdx_raid6_dsc_configure(raid_dsc, 3);
	default:
		SPDK_ERRLOG("Cannot handle raid level\n");
	}

	return -1;
}

static int rdx_dsc_create_devices(struct rdx_raid_dsc *raid_dsc,
				  struct rdx_devices *devices)
{
	struct rdx_raid *raid = raid_dsc->raid;
	int i;

	SPDK_NOTICELOG("Creating devices for raid dsc\n");

	raid_dsc->devices = calloc(raid_dsc->dev_cnt,
				sizeof(struct rdx_dev *));
	if (!raid_dsc->devices) {
		SPDK_ERRLOG("Cannot allocate memory for raid_dsc devices\n");
		return -1;
	}

	for (i = 0; i < raid_dsc->dev_cnt; i++) {
		if (raid->dsc && raid->dsc->dev_cnt > i) {
			raid_dsc->devices[i] = raid->dsc->devices[i];
			atomic_inc(&raid_dsc->devices[i]->dsc_use_cnt);
		} else if (rdx_dev_create(raid_dsc, i, devices)) {
			SPDK_ERRLOG("Cannot create device\n");
			return -1;
		}
	}

	/* Configure raid and dev sizes or validate after restoring */
	if (!raid->size) {
		for (i = 0; i < raid_dsc->dev_cnt; i++) {
			if (!raid_dsc->dev_size ||
			    (raid_dsc->devices[i]->size &&
			    raid_dsc->devices[i]->size < raid_dsc->dev_size))
				raid_dsc->dev_size = raid_dsc->devices[i]->size;
		}

		raid->size = raid_dsc->data_cnt * raid_dsc->dev_size;
		if (!raid->size) {
			SPDK_NOTICELOG("Set RAID %s size to 0\n", raid->name);
				//return -1;
		}
	} else {
		raid_dsc->dev_size = DIV_ROUND_UP(raid->size,
				raid_dsc->stripe_data_len) * raid->stripe_size;

		for (i = 0; i < raid_dsc->dev_cnt; i++) {
			if (raid_dsc->devices[i]->size &&
			    raid_dsc->devices[i]->size < raid_dsc->dev_size) {
				SPDK_ERRLOG("Device %d is too small for RAID\n", i);
				return -1;
			}
		}
	}

	return 0;
}

struct rdx_raid_dsc *rdx_raid_create_dsc(struct rdx_raid *raid,
					 int level, int dev_cnt,
					 struct rdx_devices *devices)
{
	struct rdx_raid_dsc *dsc = NULL;

	dsc = calloc(1, sizeof(struct rdx_raid_dsc));
	if(!dsc){
		SPDK_ERRLOG("Cannot allocate raid descriptor.\n");
		goto error;
	}

	dsc->raid = raid;
	dsc->dev_cnt = dev_cnt;
	dsc->level = level;

	if (rdx_dsc_configure(dsc)) {
		SPDK_ERRLOG("Cannot configure raid dsc\n");
		goto error;
	}

	dsc->stripe_len = raid->stripe_size * dsc->dev_cnt;
	dsc->stripe_data_len = raid->stripe_size * dsc->data_cnt;

	if (rdx_dsc_create_devices(dsc, devices)) {
		SPDK_ERRLOG("Cannot create devices for raid dsc\n");
		goto error;
	}

	return dsc;
error:

	rdx_raid_destroy_dsc(dsc);
	dsc = NULL;
	return dsc;
}


void rdx_raid_destroy_dsc(struct rdx_raid_dsc *raid_dsc)
{
	//rdx_dsc_destroy_devices(raid_dsc);
	//rdx_dsc_destroy_stripe_maps(raid_dsc);

	free(raid_dsc);
}
