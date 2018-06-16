#include "vbdev_common.h"
#include "vbdev_req.h"

void rdx_bdev_io_end_io(struct spdk_bdev_io *bdev_io, bool success,
			void *cb_arg)
{
	SPDK_NOTICELOG("Called end_io for bdev_io=%p\n", bdev_io);
}

struct rdx_req *rdx_req_create(struct spdk_bdev_io *bdev_io /*,
			struct rdx_blk_req *blk_req*/)
{
	struct rdx_req *req;

	req = calloc(1, sizeof(struct rdx_req));
	if (!req) {
		SPDK_ERRLOG("Cannot allocate request\n");
		return NULL;
	}

	// may be it is better to pass raid as argument?
	req->raid = (struct rdx_raid *)bdev_io->bdev->ctxt;
	req->bdev_io = bdev_io;

	req->addr = bdev_io->u.bdev.offset_blocks * RDX_BLOCK_SIZE_SECTORS;
	req->len = bdev_io->u.bdev.num_blocks * RDX_BLOCK_SIZE_SECTORS;

	req->thread_lnode.next = NULL;
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

	bdev_io->cb = rdx_bdev_io_end_io;
	return req;
}

unsigned int rdx_bdev_io_split_per_dev(struct spdk_bdev_io *bdev_io,
					int split_type)
{
	struct rdx_raid *raid = (struct rdx_raid *)bdev_io->bdev->ctxt;
	struct rdx_req *req = bdev_io->caller_ctx;
	struct rdx_dev *dev;
	struct rdx_io_ctx *io_ctx;
	unsigned int stripe_len = 16;
	unsigned int stripe_data_len = 16;
	unsigned int stripe_size = 8;
	unsigned int offset_in_strip;
	unsigned int slen, len;
	unsigned int buf_offset, buf_len;
	uint64_t addr;
	uint64_t stripe_num = 1;
	unsigned int offset_in_stripe;
	int strip_num, dev_num;
	uint64_t bdev_offset;

	addr = bdev_io->u.bdev.split_current_offset_blocks *
		RDX_BLOCK_SIZE_SECTORS;
	offset_in_stripe = addr % stripe_len;
	strip_num = offset_in_stripe / stripe_size;

	offset_in_strip = addr % stripe_size;
	slen = stripe_size - offset_in_strip;
	len = bdev_io->u.bdev.split_remaining_num_blocks * RDX_BLOCK_SIZE_SECTORS;
	//dev_num = rdx_raid_get_dev_num(raid_dsc, stripe_num, strip_num);
	//raid 0 case only
	dev_num = strip_num;
	dev = raid->devices[dev_num];

	if (slen > len)
		slen = len;

	io_ctx = calloc(1, sizeof(struct rdx_io_ctx));
	if (!io_ctx) {
		SPDK_ERRLOG("Cannot allcate io_ctx for req=%p\n", req);
		goto out_err_ctx;
	}
	io_ctx->req = req;
	io_ctx->dev = dev;

//	sector = RDX_MD_OFFSET + RDX_BACKUP_OFFSET +
//			stripe_num * stripe_size + offset_in_strip;
	bdev_offset = (stripe_num * stripe_size + offset_in_strip) *
			KERNEL_SECTOR_SIZE;
	buf_offset = (bdev_io->u.bdev.split_current_offset_blocks -
		bdev_io->u.bdev.offset_blocks) * RDX_BLOCK_SIZE;
	buf_len = slen * KERNEL_SECTOR_SIZE;
	//fix it!
	if (bdev_io->type == SPDK_BDEV_IO_TYPE_READ) {
		spdk_bdev_read(dev->base_desc,
				dev->io_channel,
				bdev_io->u.bdev.iov.iov_base + buf_offset,
				bdev_offset, buf_len,
				rdx_bdev_io_end_io, io_ctx);
	}
	if (bdev_io->type == SPDK_BDEV_IO_TYPE_WRITE) {
		spdk_bdev_write(dev->base_desc,
				dev->io_channel,
				bdev_io->u.bdev.iov.iov_base + buf_offset,
				bdev_offset, buf_len,
				rdx_bdev_io_end_io, io_ctx);
	}

	bdev_io->u.bdev.split_current_offset_blocks += slen / RDX_BLOCK_SIZE_SECTORS;
	bdev_io->u.bdev.split_remaining_num_blocks -= slen / RDX_BLOCK_SIZE_SECTORS;

	return slen;
out_err_ctx:
	//rdx_dev_put_ref(dev);
	return slen;
}
