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

#include "spdk/bdev_module.h"
#include "stddef.h"

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

struct rdx_raid_io_channel {
	struct spdk_poller *poller;
	struct rdx_raid *raid;
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
	struct spdk_io_channel *io_channel;
};

struct rdx_stripe_dsc {
	uint16_t strips_per_group;
	uint16_t groups_cnt;
	uint16_t synd_cnt; // syndromes per group
	uint16_t data_cnt;
};

struct rdx_raid {
	int stripe_size_kb;
	char *name;
	struct rdx_dev **devices;
	uint64_t size;
	uint64_t dev_size;
	int level;
	int stripe_size;
	int dev_cnt;
	struct rdx_stripe_dsc stripe_dsc;
	struct spdk_bdev_module *module;
	struct spdk_bdev raid_bdev;

	TAILQ_ENTRY(vbdev_passthru)	link;
};

static inline bool rdx_dev_is_null(char *name)
{
	if (!strncmp(name, "null", RDX_MAX_PATH_LEN))
		return true;
	return false;
}

#endif /* LIB_BDEV_RAID_VBDEV_COMMON_H_ */
