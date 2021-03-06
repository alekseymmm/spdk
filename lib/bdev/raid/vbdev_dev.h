/*
 * vbdev_dev.h
 *
 *  Created on: 21 мая 2018 г.
 *      Author: root
 */

#ifndef LIB_BDEV_RAID_VBDEV_DEV_H_
#define LIB_BDEV_RAID_VBDEV_DEV_H_

int rdx_dev_create(struct rdx_raid_dsc *raid_dsc, int dev_num,
		   struct rdx_devices *devices);
int rdx_dev_register(struct rdx_dev *dev, struct spdk_bdev *bdev);
void rdx_dev_destroy(struct rdx_dev *dev);

#endif /* LIB_BDEV_RAID_VBDEV_DEV_H_ */
