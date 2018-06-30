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
	atomic_inc(&req->ref_cnt);
}

void rdx_req_put_ref(struct rdx_req *req);
int rdx_req_complete(struct rdx_req *req);
int rdx_req_destroy(struct rdx_req *req);

void rdx_req_set_dsc(struct rdx_req *req, struct rdx_raid_dsc *raid_dsc);

#endif /* LIB_BDEV_RAID_VBDEV_REQ_H_ */
