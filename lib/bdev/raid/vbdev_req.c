#include "vbdev_common.h"
#include "vbdev_req.h"

struct rdx_req *rdx_req_create(struct spdk_bdev_io *bdev_io,
			struct rdx_blk_req *blk_req)
{
	struct rdx_req *req;

	req = calloc(1, sizeof(struct rdx_req));
	if (!req) {
		SPDK_ERRLOG("Cannot allocate request\n");
		return NULL;
	}

	return req;
}


