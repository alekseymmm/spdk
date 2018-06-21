#include "vbdev_common.h"
#include "vbdev_req.h"
#include "vbdev_blk_req.h"

static struct rdx_req *rdx_req_split(struct rdx_req *req, unsigned int len);
void rdx_bdev_io_end_io(struct spdk_bdev_io *bdev_io, bool success,
			void *cb_arg);

void rdx_bdev_io_end_io(struct spdk_bdev_io *bdev_io, bool success,
			void *cb_arg)
{
	struct rdx_io_ctx *io_ctx = cb_arg;
	struct rdx_req *req = io_ctx->req;
	struct rdx_dev *dev = io_ctx->dev;

	SPDK_DEBUG("Called for bdev_io=%p, req=%p dir=[%s], addr=%lu, len=%u\n",
		 req->bdev_io, req,
		 bdev_io->type == SPDK_BDEV_IO_TYPE_WRITE ? "W" : "R",
		 req->addr, req->len);


	if (!success) {
		SPDK_ERRLOG("bdev_io error %p code=%d req=%p bdev_name=%s\n",
		       req->bdev_io, success, req, dev->bdev_name);
		//rdx_req_err_io(req, dev, bio_data_dir(bio));
	}

	spdk_bdev_free_io(bdev_io);

	rdx_req_put_ref(req);
	//rdx_dev_put_ref(dev);
	free(io_ctx);
}

void rdx_req_set_dsc(struct rdx_req *req, struct rdx_raid_dsc *raid_dsc)
{
	//req->raid_dsc = raid_dsc;
	req->stripe_num = req->addr / 16;//raid_dsc->stripe_data_len;
	//req->stt = &raid_dsc->stt[req->type];
	//pr_debug("dsc=%p assigned for req=%p\n", raid_dsc, req);
}

struct rdx_req *rdx_req_create(struct rdx_raid *raid,
				struct spdk_bdev_io *bdev_io, void *priv,
				bool user)
{
	struct rdx_req *req;

	req = calloc(1, sizeof(struct rdx_req));
	if (!req) {
		SPDK_ERRLOG("Cannot allocate request\n");
		return NULL;
	}

	req->raid = raid;
	req->bdev_io = bdev_io;
	req->priv = priv;

	req->addr = bdev_io->u.bdev.offset_blocks * RDX_BLOCK_SIZE_SECTORS;
	req->len = bdev_io->u.bdev.num_blocks * RDX_BLOCK_SIZE_SECTORS;
	req->split_offset = 0;
	//req->bio_bvec_start_idx = bio_bi_idx(bio);
	req->buf_offset = 0;
	req->thread_lnode.next = NULL;
	atomic_init(&req->ref_cnt, 0);

	switch (bdev_io->type) {
	case SPDK_BDEV_IO_TYPE_READ:
		req->type = RDX_REQ_TYPE_READ;
		break;
	case SPDK_BDEV_IO_TYPE_WRITE:
		req->type = RDX_REQ_TYPE_WRITE;
		break;
	case SPDK_BDEV_IO_TYPE_WRITE_ZEROES:
	case SPDK_BDEV_IO_TYPE_RESET:
	case SPDK_BDEV_IO_TYPE_FLUSH:
	case SPDK_BDEV_IO_TYPE_UNMAP:
	default:
		SPDK_ERRLOG("Unsupported req type\n");
		spdk_bdev_io_complete(bdev_io, SPDK_BDEV_IO_STATUS_FAILED);
		free(req);
		req = NULL;
		break;
	}
	if (user) {
		struct rdx_blk_req *blk_req = priv;
		//set_bit(RDX_REQ_FLAG_USER, &req->flags);
		rdx_blk_req_get_ref(blk_req);
		req->ch = blk_req->ch;
		req->blk_req = blk_req;
	} else {
		rdx_req_get_ref(priv);
	}

	bdev_io->cb = rdx_bdev_io_end_io;

	SPDK_DEBUG("For bdev_io=%p created req=%p addr=%lu, len=%d,"
		" buf_offset=%lu\n", bdev_io, req, req->addr, req->len,
		req->buf_offset);
	return req;
}

unsigned int rdx_req_split_per_dev(struct rdx_req *req,
					int split_type)
{
	struct rdx_raid *raid = req->raid;
	struct spdk_bdev_io *bdev_io = req->bdev_io;
	struct rdx_dev *dev;
	struct rdx_io_ctx *io_ctx;
	struct spdk_io_channel *io_channel;
	unsigned int stripe_len = 16;
	unsigned int stripe_data_len = 16;
	unsigned int stripe_size = 8;
	unsigned int offset_in_strip;
	unsigned int slen, len;
	unsigned int split_offset, buf_len;
	uint64_t addr;
	uint64_t stripe_num = req->stripe_num;
	unsigned int offset_in_stripe;
	int strip_num, dev_num;
	uint64_t bdev_offset;
	char *io_buf;

	addr = req->addr + req->split_offset;
	offset_in_stripe = addr % stripe_len;
	strip_num = offset_in_stripe / stripe_size;

	offset_in_strip = addr % stripe_size;
	slen = stripe_size - offset_in_strip;
	len = req->len - req->split_offset;
	//dev_num = rdx_raid_get_dev_num(raid_dsc, stripe_num, strip_num);
	//raid 0 case only
	dev_num = strip_num;
	dev = raid->devices[dev_num];
	io_channel = req->ch->dev_io_channels[dev_num];

	if (slen > len)
		slen = len;

	io_ctx = calloc(1, sizeof(struct rdx_io_ctx));
	if (!io_ctx) {
		SPDK_ERRLOG("Cannot allocate io_ctx for req=%p\n", req);
		goto out_err_ctx;
	}
	io_ctx->req = req;
	io_ctx->dev = dev;

//	sector = RDX_MD_OFFSET + RDX_BACKUP_OFFSET +
//			stripe_num * stripe_size + offset_in_strip;
	bdev_offset = (stripe_num * stripe_size + offset_in_strip) *
			KERNEL_SECTOR_SIZE;
	split_offset = req->split_offset * KERNEL_SECTOR_SIZE;
	buf_len = slen * KERNEL_SECTOR_SIZE;
	io_buf = bdev_io->u.bdev.iov.iov_base + req->buf_offset + split_offset;

	rdx_req_get_ref(req);

	if (bdev_io->type == SPDK_BDEV_IO_TYPE_READ) {
		spdk_bdev_read(dev->base_desc,
				io_channel,
				io_buf,
				bdev_offset, buf_len,
				rdx_bdev_io_end_io, io_ctx);
	}
	if (bdev_io->type == SPDK_BDEV_IO_TYPE_WRITE) {
		spdk_bdev_write(dev->base_desc,
				io_channel,
				io_buf,
				bdev_offset, buf_len,
				rdx_bdev_io_end_io, io_ctx);
	}

	req->split_offset += slen;

	return slen;
out_err_ctx:
	//rdx_dev_put_ref(dev);
	return slen;
}

static struct rdx_req *rdx_req_split(struct rdx_req *req, unsigned int len)
{
	struct rdx_req *split_req;

	split_req = rdx_req_create(req->raid, req->bdev_io, req->priv, true);
	if (!split_req) {
		//bio_put(split_bio);
		SPDK_ERRLOG("Cannot allocate memory for splitted req\n");
		return NULL;
	}
	split_req->len = len;
	split_req->addr = req->addr;
	split_req->buf_offset = req->buf_offset;
	req->addr += len;
	req->len -= len;
	req->buf_offset += len * KERNEL_SECTOR_SIZE;
	//req->bio_bvec_start_idx = bio_bi_idx(req->bio);
	//split_req->flags = req->flags;

	return split_req;
}
//split req per stripes and submit requests
void rdx_req_split_per_stripe(struct rdx_req *req)
{
	uint64_t addr;
	unsigned int slen, len;
	unsigned int stripe_data_len, offset_in_stripe;
	unsigned int sectors_to_split;
	struct rdx_req *split_req;

	sectors_to_split = req->len;
	while (sectors_to_split) {
		addr = req->addr;
		len = req->len;

//		raid_dsc = rdx_req_get_raid_dsc(req);
//
//		if (!raid_dsc) {
//			req->restripe_lnode.next = NULL;
//			llist_add(&req->restripe_lnode, &req->raid->restripe_pending_list);
//			pr_debug("req=%p goes to group locked for restriping. "
//				  "Add  to pending list.\n", req);
//			/* TODO:
//			 * We have a race, if restriping process
//			 * finished. Who will handle this req in
//			 * pending list?
//			 * This race was fixed by hack with msleep in
//			 * rdx_restripe_stop().
//			 */
//			return;
//		}

		stripe_data_len = 16;//raid_dsc->stripe_data_len;
		offset_in_stripe = addr % stripe_data_len;
		slen = stripe_data_len - offset_in_stripe;
		//test
		//slen=len;
		if (slen >= len) {
			slen = len;
			split_req = req;
		} else {

			split_req = rdx_req_split(req, slen);

			/* Error handling is too complex, but it works */
			if (!split_req) {
//				struct rdx_blk_req *blk_req = req->priv;
//				bio_put(req->bio);
//				blk_req->err = -EIO;
//				rdx_blk_req_put_ref(blk_req);
//				if (test_bit(RDX_REQ_FLAG_IN_RESTRIPE, &req->flags))
//					rdx_req_put_raid_dsc(req);
//				kmem_cache_free(rdx_req_cachep, req);
				return;
			}
			SPDK_DEBUG("req=%p splitted. splitteq=%p addr=%lu,"
				   " len=%d buf_offset=%lu. Initial req=%p,"
				   " addr=%lu, len=%d, buf_offset=%lu\n",
				   req, split_req, split_req->addr,
				   split_req->len, split_req->buf_offset, req,
				   req->addr, req->len, req->buf_offset);

		}

		rdx_req_set_dsc(split_req, NULL);
//
//		rdx_req_submit(split_req);
		llist_add(&split_req->thread_lnode, &split_req->ch->req_llist);

		sectors_to_split -= slen;
	}
}

static void rdx_req_end_io(struct rdx_req *req)
{
//	struct rdx_raid_dsc *raid_dsc = req->raid_dsc;
//	int failed_cnt = __builtin_popcountll(req->failed_bitmap);
//	bool can_process;
//
//	if (!rdx_raid_can_recover(raid_dsc, failed_cnt))
//		req->event = RDX_REQ_EVENT_XFER_FAILED;
//	else if (req->state == RDX_REQ_STATE_READ)
//		rdx_req_end_io_read(req);
//	else if (req->state == RDX_REQ_STATE_WRITE)
//		rdx_req_end_io_write(req);
//
//	if (req->event == RDX_REQ_EVENT_XFER_FAILED) {
//		pr_err("Req failed %p, type %d, failed cnt %d, fd_mask=%llu\n",
//		       req, req->type, failed_cnt, req->failed_bitmap);
//		set_bit(RDX_REQ_FLAG_ERR, &req->flags);
//	}
//
//	/* Clear specific flags after each xfer step */
//	clear_bit(RDX_REQ_FLAG_BIO_ERR, &req->flags);
//
//	/* Allow FRONT READ and WRITE requests to complete in process context */
//	can_process = (req->type == RDX_REQ_TYPE_READ &&
//			req->event == RDX_REQ_EVENT_XFER_COMPLETED &&
//			test_bit(RDX_REQ_FLAG_USER, &req->flags)) ||
//			(req->type == RDX_REQ_TYPE_WRITE &&
//			req->state == RDX_REQ_STATE_WRITE);
//	if (can_process)
//		rdx_req_process(req);
//	else
//		rdx_thread_queue_work(req->thread_num, &req->thread_lnode);
	//test
	rdx_req_complete(req);
}

void rdx_req_put_ref(struct rdx_req *req)
{
	SPDK_DEBUG("for req=%p before dec_and_test ref_cnt=%d\n",
			req, atomic_load(&req->ref_cnt));

	if (__atomic_sub_fetch(&req->ref_cnt, 1, memory_order_seq_cst) == 0) {
		SPDK_DEBUG("For req=%p achieved ref_cnt=0, complete it\n",
			 req);

		rdx_req_end_io(req);
	}
}

int rdx_req_complete(struct rdx_req *req)
{
	int err =0;
	//int err = test_bit(RDX_REQ_FLAG_ERR, &req->flags);

	if (1 /*test_bit(RDX_REQ_FLAG_USER, &req->flags*/) {
		struct rdx_blk_req *blk_req = req->blk_req;//req->priv;
		if (err)
			blk_req->err = -EIO;
		rdx_blk_req_put_ref(blk_req);
	} else {
//		struct rdx_req *priv = req->priv;
//		if (err)
//			bitmap_fill((unsigned long *)&req->failed_bitmap,
//				    req->raid_dsc->dev_cnt);
//		rdx_req_put_ref(priv);
	}

	//bio_put(req->bio);

//	req->event = RDX_REQ_EVENT_COMPLETED;
//	rdx_thread_queue_work(req->thread_num, &req->thread_lnode);
	rdx_req_destroy(req);

	return 0;
}

int rdx_req_destroy(struct rdx_req *req)
{
//	struct rdx_raid *raid = req->raid;
//
//	if (req->stripe)
//		rdx_req_release(req);
//
//	rdx_raid_threads_map_put(req->raid, req);
//
//	if (test_bit(RDX_REQ_FLAG_IN_STATS, &req->flags))
//		atomic_dec(&raid->stats->req_cnt[req->type]);
//
//	if (test_bit(RDX_REQ_FLAG_IN_RESTRIPE, &req->flags))
//		rdx_req_put_raid_dsc(req);
//
//	kmem_cache_free(rdx_req_cachep, req);
	free(req);

	return 0;
}
