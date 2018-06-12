#include "vbdev_common.h"
#include "vbdev_req.h"

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
	//TODO: convert to linux kernel block size
	req->addr = bdev_io->u.bdev.offset_blocks;
	req->len = bdev_io->u.bdev.num_blocks;

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

	return req;
}


