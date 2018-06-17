/*
 * vbdev_blk_req.h
 *
 *  Created on: Jun 17, 2018
 *      Author: root
 */

#ifndef LIB_BDEV_RAID_VBDEV_BLK_REQ_H_
#define LIB_BDEV_RAID_VBDEV_BLK_REQ_H_

void rdx_blk_submit(struct rdx_blk_req *blk_req);

static inline void rdx_blk_req_get_ref(struct rdx_blk_req *blk_req)
{
	atomic_fetch_add(&blk_req->ref_cnt, 1);
}

#endif /* LIB_BDEV_RAID_VBDEV_BLK_REQ_H_ */
