/*
 * vbdev_blk_req.c
 *
 *  Created on: Jun 17, 2018
 *      Author: root
 */

#include "vbdev_common.h"
#include "vbdev_req.h"
#include "vbdev_blk_req.h"

void rdx_blk_submit(struct rdx_blk_req *blk_req)
{
	struct rdx_req *req;

	rdx_blk_req_get_ref(blk_req);

	req = rdx_req_create(blk_req->raid, blk_req->bdev_io, blk_req, true);
	if (!req) {
		//bio_put(clone);
		blk_req->err = -EIO;
		SPDK_ERRLOG("Cannot create rdx_req\n");
	}

	rdx_req_split_per_stripe(req);

	//rdx_blk_req_put_ref(blk_req);
}
