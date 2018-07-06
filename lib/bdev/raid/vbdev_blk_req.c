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
	rdx_blk_req_put_ref(blk_req);
/*
	req = rdx_req_create(blk_req->raid, blk_req->bdev_io, blk_req, true);
	if (!req) {
		//bio_put(clone);
		blk_req->err = -EIO;
		SPDK_ERRLOG("Cannot create rdx_req\n");
	}
//TEST
	//rdx_req_split_per_stripe(req);
	rdx_req_get_ref(req);
	rdx_req_put_ref(req);
	rdx_blk_req_put_ref(blk_req);
	*/

}


void rdx_blk_req_put_ref(struct rdx_blk_req *blk_req)
{
	SPDK_DEBUG("Called for blk_req=%p dir=[%s], addr=%lu, len=%u, "
		 "Before dec_and_test blk_req->ref_cnt=%d\n",
		 blk_req, blk_req->rw == SPDK_BDEV_IO_TYPE_WRITE ? "W" : "R",
		 blk_req->addr, blk_req->len, atomic_load(&blk_req->ref_cnt));

	if (__atomic_sub_fetch(&blk_req->ref_cnt, 1, memory_order_seq_cst) == 0) {
		SPDK_DEBUG("For blk_req=%p achieved ref_cnt=0, complete it, code=%d\n",
			 blk_req, blk_req->err);

		blk_req->bdev_io->internal.cb = blk_req->bdev_io->u.bdev.stored_user_cb;
		SPDK_DEBUG("complete bdev_io=%p \n", blk_req->bdev_io);
		spdk_bdev_io_complete(blk_req->bdev_io, SPDK_BDEV_IO_STATUS_SUCCESS);
		//free(blk_req);
		//spdk_mempool_put(blk_req->raid->blk_req_mempool, blk_req);
		//spdk_mempool_put(blk_req->mempool, blk_req);
		llist_add(&blk_req->pool_lnode, &blk_req->ch->blk_req_llist);
	}
}
