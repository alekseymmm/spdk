/*
 * vbdev_req.h
 *
 *  Created on: Jun 12, 2018
 *      Author: root
 */

#ifndef LIB_BDEV_RAID_VBDEV_REQ_H_
#define LIB_BDEV_RAID_VBDEV_REQ_H_

struct rdx_req *rdx_req_create(struct rdx_raid *raid,
				struct spdk_bdev_io *bdev_io, void *priv,
				bool user);
unsigned int rdx_req_split_per_dev(struct rdx_req *req,
					int split_type);
void rdx_req_split_per_stripe(struct rdx_req *req);

static inline void rdx_req_get_ref(struct rdx_req *req)
{
	atomic_fetch_add(&req->ref_cnt, 1);
}
#endif /* LIB_BDEV_RAID_VBDEV_REQ_H_ */
