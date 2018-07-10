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
	rdx_blk_req_put_ref(blk_req);


}


void rdx_blk_req_put_ref(struct rdx_blk_req *blk_req)
{
	if (__atomic_sub_fetch(&blk_req->ref_cnt, 1, memory_order_seq_cst) == 0) {
		blk_req->bdev_io->internal.cb = blk_req->bdev_io->u.bdev.stored_user_cb;
		spdk_bdev_io_complete(blk_req->bdev_io, SPDK_BDEV_IO_STATUS_SUCCESS);
		//free(blk_req);
		//spdk_mempool_put(blk_req->raid->blk_req_mempool, blk_req);
		spdk_mempool_put(blk_req->mempool, blk_req);
		//llist_add(&blk_req->pool_lnode, &blk_req->ch->blk_req_llist);
	}
}
