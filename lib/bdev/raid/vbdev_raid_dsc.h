/*
 * vbdev_raid_dsc.h
 *
 *  Created on: Jun 30, 2018
 *      Author: root
 */

#ifndef LIB_BDEV_RAID_VBDEV_RAID_DSC_H_
#define LIB_BDEV_RAID_VBDEV_RAID_DSC_H_

struct rdx_raid_dsc *rdx_raid_create_dsc(struct rdx_raid *raid,
					 int level, int dev_cnt,
					 struct rdx_devices *devices);

void rdx_raid_destroy_dsc(struct rdx_raid_dsc *raid_dsc);

#endif /* LIB_BDEV_RAID_VBDEV_RAID_DSC_H_ */
