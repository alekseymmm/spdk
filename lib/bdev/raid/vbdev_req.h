/*
 * vbdev_req.h
 *
 *  Created on: Jun 12, 2018
 *      Author: root
 */

#ifndef LIB_BDEV_RAID_VBDEV_REQ_H_
#define LIB_BDEV_RAID_VBDEV_REQ_H_

struct rdx_req *rdx_req_create(struct spdk_bdev_io *bdev_io /*,
			struct rdx_blk_req *blk_req*/);
unsigned int rdx_bdev_io_split_per_dev(struct spdk_bdev_io *bdev_io,
					int split_type);
#endif /* LIB_BDEV_RAID_VBDEV_REQ_H_ */
