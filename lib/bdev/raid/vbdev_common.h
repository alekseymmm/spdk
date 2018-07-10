/*
 * vbdev_common.h
 *
 *  Created on: 17 мая 2018 г.
 *      Author: alekseym
 */

#ifndef LIB_BDEV_RAID_VBDEV_COMMON_H_
#define LIB_BDEV_RAID_VBDEV_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "spdk/bdev.h"
#include "spdk/log.h"
#include "spdk/env.h"

#include "spdk/bdev_module.h"
#include "stddef.h"

#include "llist.h"

extern struct rdx_raid *g_raid;
extern struct spdk_bdev_module raid_if;

#define RDX_MD_OFFSET 0

#define KERNEL_SECTOR_SIZE_SHIFT (9)
#define KERNEL_SECTOR_SIZE (1 << KERNEL_SECTOR_SIZE_SHIFT)

#define RDX_MAX_DEV_CNT 64
#define RDX_MAX_PATH_LEN 256
#define RDX_MAX_NAME_LEN 32

#define RDX_SECTOR_SIZE 512
#define RDX_BLOCK_SIZE 4096
#define RDX_BLOCK_SIZE_SECTORS (RDX_BLOCK_SIZE / KERNEL_SECTOR_SIZE)

#define RDX_REQ_POOL_SIZE			(64 * 1024)
#define RDX_REQ_CACHE_SIZE			256

#define RDX_BLK_REQ_POOL_SIZE			(64 * 1024)
#define RDX_BLK_REQ_CACHE_SIZE			256

#define RDX_IO_CTX_POOL_SIZE			(64 * 1024)
#define RDX_IO_CTX_CACHE_SIZE			256

enum rdx_raid_state_shift {
	RDX_RAID_STATE_ONLINE_SHIFT = 0,
	RDX_RAID_STATE_DEGRADED_SHIFT,
	RDX_RAID_STATE_CREATE_SHIFT,
	RDX_RAID_STATE_DESTROY_SHIFT,
	RDX_RAID_STATE_RECON_SHIFT,
	RDX_RAID_STATE_NEED_RECON_SHIFT,
	RDX_RAID_STATE_STOP_RECON_SHIFT,
	RDX_RAID_STATE_INIT_SHIFT,
	RDX_RAID_STATE_NEED_INIT_SHIFT,
	RDX_RAID_STATE_STOP_INIT_SHIFT,
	RDX_RAID_STATE_RESTRIPING_SHIFT,
	RDX_RAID_STATE_NEED_RESTRIPING_SHIFT,
};


#define RDX_RAID_STATE_ONLINE (1 << RDX_RAID_STATE_ONLINE_SHIFT)
#define RDX_RAID_STATE_DEGRADED (1 << RDX_RAID_STATE_DEGRADED_SHIFT)
#define RDX_RAID_STATE_CREATE (1 << RDX_RAID_STATE_CREATE_SHIFT)

enum rdx_req_type {
	RDX_REQ_TYPE_READ = 0,
	RDX_REQ_TYPE_WRITE,
	RDX_REQ_TYPE_RECON,
	RDX_REQ_TYPE_INIT,
	RDX_REQ_TYPE_RESTRIPE,
	RDX_REQ_TYPE_BACKUP,
	RDX_REQ_TYPE_COUNT,
};

struct rdx_raid_io_channel {
	struct spdk_poller *poller;
	struct rdx_raid *raid;
	struct llist_head req_llist;
	struct spdk_io_channel **dev_io_channels;

	struct spdk_mempool *blk_req_mempool;
	struct spdk_mempool *req_mempool;
	struct rdx_blk_req *blk_req_pool;
	struct rdx_req *req_pool;
	int blk_req_pool_size;
	int req_pool_size;
	struct llist_head blk_req_llist;
	//struct llist_head req_llist;
	//something else
};

struct rdx_devices {
	char **names;
	int cnt;
};

struct rdx_dev {
	uint64_t size;
	int num;
	struct spdk_bdev *bdev;
	struct rdx_raid *raid;
	char *bdev_name;
	struct spdk_bdev_desc *base_desc;
	int dsc_use_cnt;
};

struct rdx_raid {
	int stripe_size_kb;
	char *name;
	struct rdx_raid_dsc *dsc;
	uint64_t size;
	int level;
	int stripe_size;
	struct spdk_bdev_module *module;
	struct spdk_bdev raid_bdev;
	struct spdk_mempool *req_mempool;
	struct spdk_mempool *blk_req_mempool;
	struct spdk_mempool *io_ctx_mempool;
	TAILQ_ENTRY(vbdev_passthru)	link;
};

struct rdx_raid_dsc {
	struct rdx_raid *raid;
	struct rdx_dev **devices;
	uint64_t dev_size;//TODO raid shrinking
	uint16_t strips_per_substripe;
	uint16_t substripes_cnt;
	uint16_t synd_cnt; // syndromes per substripe
	uint16_t data_cnt;
	unsigned int stripe_len; //number of drives * stripe_size
	unsigned int stripe_data_len; // number of data_drives * stripe_size
	int dev_cnt;
	uint64_t stripe_cnt;
	int level;
};

struct rdx_req {
	uint64_t addr; //sectors
	unsigned int len; //sectors
	unsigned int split_offset; //sectors
	uint64_t buf_offset; //bytes
	struct rdx_raid *raid;
	struct spdk_bdev_io *bdev_io;
	struct llist_node thread_lnode;
	enum rdx_req_type type;
	void *priv;
	struct rdx_raid_io_channel *ch;
	struct rdx_blk_req *blk_req;
	int ref_cnt;
	uint64_t stripe_num;
	struct rdx_raid_dsc *raid_dsc;
	struct llist_node pool_lnode;
};

struct rdx_blk_req {
	uint64_t addr;
	unsigned int len;	/* sectors */
	int rw;
	//atomic_int ref_cnt;
	int ref_cnt;
	struct rdx_raid *raid;
	struct spdk_bdev_io *bdev_io;
	int err;
	struct rdx_raid_io_channel *ch;
	struct spdk_mempool *mempool;
	struct llist_node pool_lnode;
};

struct rdx_io_ctx {
	union {
		/* used for generic requests */
		struct rdx_req *req;
		/* used for metadata requests */
		//struct md_io_ctx *md_ctx;
	};
	struct rdx_dev *dev;
};

static inline bool rdx_dev_is_null(char *name)
{
	if (!strncmp(name, "null", RDX_MAX_PATH_LEN))
		return true;
	return false;
}

int rdx_raid0_dsc_configure(struct rdx_raid_dsc *raid_dsc);
//static inline int rdx_raid_get_dev_num(struct rdx_raid_dsc *raid_dsc,
//					u64 stripe_num, int strip_num)
//{
//	int dev_num = 0;
//	int left_shift;
//
//	switch (raid_dsc->level) {
//	case 0:
//	case 1 :
//		dev_num = strip_num;
//		break;
//	case 5:
//	case 6:
//	case 7:
//		left_shift = stripe_num % raid_dsc->dev_cnt;
//		dev_num = (strip_num + raid_dsc->dev_cnt - left_shift) %
//				raid_dsc->dev_cnt;
//		break;
//	}
//
//	return dev_num;
//}
#define atomic_inc(PTR) atomic_fetch_add(PTR, 1)

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#endif /* LIB_BDEV_RAID_VBDEV_COMMON_H_ */
