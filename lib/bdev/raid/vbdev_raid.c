/*
 * vbdev_raid.c
 *
 *  Created on: 18 мая 2018 г.
 *      Author: root
 */

#include <stdatomic.h>
#include "spdk/bdev.h"
#include "spdk/log.h"

#include "spdk/bdev_module.h"

#include "vbdev_common.h"
#include "vbdev_dev.h"
#include "vbdev_raid.h"
#include "vbdev_raid_dsc.h"

#include "vbdev_req.h"
#include "vbdev_blk_req.h"
#include "llist.h"

int blk_req_pool_iter = 0;
int req_pool_iter = 0;
int io_ctx_pool_iter = 0;

static void vbdev_raid_submit_request(struct spdk_io_channel *_ch,
				struct spdk_bdev_io *bdev_io);
static struct spdk_io_channel *vbdev_raid_get_io_channel(void *ctx);
static int vbdev_raid_destruct(void *ctx);

static void vbdev_raid_write_json_config(struct spdk_bdev *bdev,
					 struct spdk_json_write_ctx *w);


/* When we regsiter our bdev this is how we specify our entry points. */
static const struct spdk_bdev_fn_table vbdev_raid_fn_table = {
	.destruct		= vbdev_raid_destruct,
	.submit_request		= vbdev_raid_submit_request,
	.io_type_supported	= NULL,//vbdev_passthru_io_type_supported,
	.get_io_channel		= vbdev_raid_get_io_channel,
	.dump_info_json		= NULL,//vbdev_passthru_info_config_json,
	.write_config_json	= vbdev_raid_write_json_config,//
};

int spdk_raid_create(char *name, int level, int stripe_size_kb,
		struct rdx_devices *devices, uint64_t raid_size)
{
	g_raid = calloc(1, sizeof(struct rdx_raid));
	if (!g_raid) {
		SPDK_ERRLOG("Cannot allocate memory\n");
		return -ENOMEM;
	}

	g_raid->name = strdup(name);
	g_raid->level = level;
	g_raid->stripe_size_kb = stripe_size_kb;
	g_raid->stripe_size = stripe_size_kb * 1024 / KERNEL_SECTOR_SIZE;
	g_raid->size = raid_size;
	g_raid->module = &raid_if;

	g_raid->raid_bdev.name = strdup(name);
	g_raid->raid_bdev.product_name = "raid";
	g_raid->raid_bdev.write_cache = 0;
	g_raid->raid_bdev.need_aligned_buffer = 1; // test 0
	g_raid->raid_bdev.optimal_io_boundary = 1; // test 0
	g_raid->raid_bdev.blocklen = RDX_BLOCK_SIZE;
	g_raid->raid_bdev.blockcnt = raid_size / RDX_BLOCK_SIZE_SECTORS;
	// not so sure about this,
	// why not ? it seems fine.  You can get ptr to raid, having ptr to bdev
	// for example in submit request. Though you can alose get ptr to raid
	// from io_cahnnel there...
	g_raid->raid_bdev.ctxt = g_raid;

	g_raid->raid_bdev.fn_table = &vbdev_raid_fn_table;
	g_raid->raid_bdev.module = &raid_if;
//	if (!raid_size)
//		set_bit(RDX_RAID_STATE_CREATE, &raid->state);

	g_raid->dsc = rdx_raid_create_dsc(g_raid, level, devices->cnt, devices);
	if (!g_raid->dsc) {
		SPDK_ERRLOG("Cannot create descriptor for raid %s\n", g_raid->name);
		goto error;
	}

	return 0;
error:
	vbdev_raid_destruct(g_raid);
	return -ENOMEM;
}

int rdx_raid_replace(struct rdx_raid_dsc *raid_dsc, int dev_num,
		struct spdk_bdev *bdev)
{
	int ret = 0;

	ret = rdx_dev_register(raid_dsc->devices[dev_num], bdev);
	if (ret) {
		SPDK_ERRLOG("Failed to replace device %s on position %d\n",
				bdev->name, dev_num);
		return -1;
	}
	SPDK_NOTICELOG("raid %s device=%s replaced on pos %d\n",
			raid_dsc->raid->name, bdev->name, dev_num);

	return 0;
}

static void vbdev_raid_submit_request(struct spdk_io_channel *_ch,
				struct spdk_bdev_io *bdev_io)
{
	struct rdx_raid_io_channel *ch = spdk_io_channel_get_ctx(_ch);
	struct rdx_blk_req *blk_req;

	blk_req = spdk_mempool_get(ch->blk_req_mempool);
	if (!blk_req) {
		SPDK_ERRLOG("Cannot allocate blk_req for bdev_io\n");
		return;
	}

	blk_req->mempool = ch->blk_req_mempool;
	blk_req->addr = bdev_io->u.bdev.offset_blocks * RDX_BLOCK_SIZE_SECTORS;
	blk_req->len = bdev_io->u.bdev.num_blocks * RDX_BLOCK_SIZE_SECTORS;
	blk_req->rw = bdev_io->type;
	blk_req->bdev_io = bdev_io;
	blk_req->raid = ch->raid;
	blk_req->ch = ch;
	atomic_init(&blk_req->ref_cnt, 0);

	bdev_io->u.bdev.stored_user_cb = bdev_io->internal.cb;

	rdx_blk_submit(blk_req);
}

static int vbdev_raid_poll(void *arg)
{
	struct rdx_raid_io_channel *ch = arg;
	struct rdx_req *req;
	struct llist_node *first, *next;
	unsigned int sectors_to_split, len = 0;	

	first = llist_del_all(&ch->req_llist);
	while (first) {
		next = first->next;
		req = llist_entry(first, struct rdx_req, thread_lnode);

//		req->bdev_io->cb = req->bdev_io->u.bdev.stored_user_cb;
//
//		spdk_bdev_io_complete(req->bdev_io, 1);
		first = next;
		sectors_to_split = req->len;
		req->split_offset = 0;
		while (sectors_to_split) {
			//len = rdx_bdev_io_split_per_dev(req->bdev_io, 0);
			len = rdx_req_split_per_dev(req, 0);
			sectors_to_split -= len;
		}

	}
//	SPDK_NOTICELOG("In raid_io_channel %p processed %d requests\n", ch, processed);

	//spdk_bdev_io_complete(req->bdev_io, SPDK_BDEV_IO_STATUS_SUCCESS);

	return 0;
}


//static int vbdev_raid_poll(void *arg)
//{
//	struct rdx_raid_io_channel *ch = arg;
//	struct rdx_req *req;
//	struct llist_node *first;
//	unsigned int sectors_to_split, len = 0;
//
//	if (!llist_empty(&ch->req_llist)) {
//		first = llist_del_first(&ch->req_llist);
//		req = llist_entry(first, struct rdx_req, thread_lnode);
//
//		req->bdev_io->cb = req->bdev_io->u.bdev.stored_user_cb;
//		spdk_bdev_io_complete(req->bdev_io, 1);
////		sectors_to_split = req->len;
////		req->split_offset = 0;
////		while (sectors_to_split) {
////			//len = rdx_bdev_io_split_per_dev(req->bdev_io, 0);
////			len = rdx_req_split_per_dev(req, 0);
////			sectors_to_split -= len;
////		}
//
//	}
//
//	//spdk_bdev_io_complete(req->bdev_io, SPDK_BDEV_IO_STATUS_SUCCESS);
//
//	return 0;
//}
/* We provide this callback for the SPDK channel code to create a channel using
 * the channel struct we provided in our module get_io_channel() entry point. Here
 * we get and save off an underlying base channel of the device below us so that
 * we can communicate with the base bdev on a per channel basis.  If we needed
 * our own poller for this vbdev, we'd register it here.
 */
static int
raid_bdev_ch_create_cb(void *io_device, void *ctx_buf)
{

	struct rdx_raid_io_channel *raid_ch = ctx_buf;
	struct rdx_raid *raid = io_device;
	struct rdx_raid_dsc *raid_dsc = raid->dsc;
	int i;
	char name[32];

	raid_ch->raid = raid;
	//raid_ch->poller = spdk_poller_register(vbdev_raid_poll, raid_ch, 0);
	init_llist_head(&raid_ch->req_llist);

	raid_ch->dev_io_channels = calloc(raid->dsc->dev_cnt,
				sizeof(struct spdk_io_channel *));
	if (!raid_ch->dev_io_channels) {
		SPDK_ERRLOG("Cannot allocate base bdevs io_channels array\n");
		return -1;
	}

	for (i = 0; i < raid->dsc->dev_cnt; i++) {
		raid_ch->dev_io_channels[i] = spdk_bdev_get_io_channel(
						raid_dsc->devices[i]->base_desc);
		if (!raid_ch->dev_io_channels[i]){
			SPDK_ERRLOG("could not open io_channel for bdev%s\n",
				spdk_bdev_get_name(raid_dsc->devices[i]->bdev));

//				spdk_bdev_close(dev->base_desc);
//				spdk_bdev_module_release_bdev(dev->bdev);
//				goto error;
		}
	}

	snprintf(name, 32, "blk_req%d", atomic_fetch_add(&blk_req_pool_iter, 1));

	// We can probably pass spdk_env_get_socket_id(spdk_env_get_current_core())
	// instead of SPDK_ENV_SOCKET_ID_ANY, but it only works for EAL threads
	// not fio threads (pthreads)
	raid_ch->blk_req_mempool = spdk_mempool_create(name,
			512, sizeof(struct rdx_blk_req),
			SPDK_MEMPOOL_DEFAULT_CACHE_SIZE,
			SPDK_ENV_SOCKET_ID_ANY);

	if (!raid_ch->blk_req_mempool) {
		SPDK_ERRLOG("Cannot create mempool %s for channel %p\n", name,
				raid_ch);
		return -1;
	}

	snprintf(name, 32, "req%d", atomic_fetch_add(&req_pool_iter, 1));
	raid_ch->req_mempool = spdk_mempool_create(name,
			512, sizeof(struct rdx_req),
			SPDK_MEMPOOL_DEFAULT_CACHE_SIZE,
			SPDK_ENV_SOCKET_ID_ANY);
	if (!raid_ch->req_mempool) {
		SPDK_ERRLOG("Cannot create mempool %s for channel %p\n", name,
				raid_ch);
		return -1;
	}

	snprintf(name, 32, "io_ctx%d", atomic_fetch_add(&io_ctx_pool_iter, 1));
	raid_ch->io_ctx_mempool = spdk_mempool_create(name,
			512, sizeof(struct rdx_io_ctx),
			SPDK_MEMPOOL_DEFAULT_CACHE_SIZE,
			SPDK_ENV_SOCKET_ID_ANY);
	if (!raid_ch->io_ctx_mempool) {
		SPDK_ERRLOG("Cannot create mempool %s for channel %p\n", name,
				raid_ch);
		return -1;
	}

	return 0;
}

/* We provide this callback for the SPDK channel code to destroy a channel
 * created with our create callback. We just need to undo anything we did
 * when we created. If this bdev used its own poller, we'd unregsiter it here.
 */
static void
raid_bdev_ch_destroy_cb(void *io_device, void *ctx_buf)
{
	int i;
	struct rdx_raid_io_channel *raid_ch = ctx_buf;
	struct rdx_raid *raid = raid_ch->raid;

	for (i = 0; i < raid->dsc->dev_cnt; i++) {
		spdk_put_io_channel(raid_ch->dev_io_channels[i]);
	}
	free(raid_ch->dev_io_channels);
	spdk_poller_unregister(&raid_ch->poller);

	if (raid_ch->blk_req_mempool)
		spdk_mempool_free(raid_ch->blk_req_mempool);
	if (raid_ch->req_mempool)
		spdk_mempool_free(raid_ch->req_mempool);
	if (raid_ch->io_ctx_mempool)
		spdk_mempool_free(raid_ch->io_ctx_mempool);
}


static struct spdk_io_channel *vbdev_raid_get_io_channel(void *ctx)
{
	struct rdx_raid *raid = ctx;
	struct spdk_io_channel *io_ch = NULL;
	io_ch = spdk_get_io_channel(raid);;
	return io_ch;
}

int rdx_raid_register(struct rdx_raid *raid)
{
	struct spdk_bdev **base_bdevs;
	struct rdx_raid_dsc *raid_dsc = raid->dsc;
	int rc = 0;
	int i;

	SPDK_NOTICELOG("init complete called for raid module\n");
	base_bdevs = calloc(raid_dsc->dev_cnt, sizeof(struct spdk_bdev *));
	if (!base_bdevs) {
		SPDK_ERRLOG("Cannot allcoate memory for bdevs list\n");
		return -1;
	}

	if (raid->size == 0) {
		raid->size = raid_dsc->dev_size * raid_dsc->dev_cnt;
		raid->raid_bdev.blockcnt = raid->size / RDX_BLOCK_SIZE_SECTORS;
	}
	SPDK_NOTICELOG("Registering raid %s of size %lu\n",
			raid->name , raid->size);
	spdk_io_device_register(raid, raid_bdev_ch_create_cb,
				raid_bdev_ch_destroy_cb,
				sizeof(struct rdx_raid_io_channel));

	SPDK_NOTICELOG("io_device  for raid registered \n");

	for (i = 0; i < raid_dsc->dev_cnt; i++) {
		base_bdevs[i] = raid_dsc->devices[i]->bdev;
	}

	//This is not working doe to some bug in SPDK. (see github)
	//rc = spdk_vbdev_register(&g_raid->raid_bdev, base_bdevs, g_raid->dev_cnt);
	rc = spdk_bdev_register(&g_raid->raid_bdev);
	if (rc) {
		SPDK_ERRLOG("Failed to register vbdev raid\n");
		goto error;
	}
	SPDK_NOTICELOG("vbdev raid registered with name: %s\n",
			raid->raid_bdev.name);

error:
	free(base_bdevs);

	return rc;
}

//int rdx_raid_unregister(struct rdx_raid *raid)
//{
//	spdk_io_device_unregister(&raid->raid_bdev, NULL);
//}

/* Called after we've unregistered following a hot remove callback.
 * Our finish entry point will be called next.
 */
static int vbdev_raid_destruct(void *ctx)
{

	struct rdx_raid *raid = (struct rdx_raid *)ctx;

	SPDK_NOTICELOG("Destruct raid %s\n", raid->name);

	if (raid->dsc)
		rdx_raid_destroy_dsc(raid->dsc);

	free(raid->name);
	free(raid);

	//FO not do this stuff here, or add some checks
		//spdk_io_device_unregister(raid, NULL);
		//spdk_bdev_unregister(&raid->raid_bdev, NULL, NULL);
		//spdk_bdev_unregister(&g_raid->raid_bdev, NULL, NULL);
	// we have to chek if it is already unregistered or scheduled remove:wq
	//spdk_bdev_unregister(&raid->raid_bdev, NULL, NULL);

	return 0;
}

/* Called when SPDK wants to output the bdev specific methods. */
static void
vbdev_raid_write_json_config(struct spdk_bdev *bdev, struct spdk_json_write_ctx *w)
{
	//struct vbdev_passthru *pt_node = SPDK_CONTAINEROF(bdev, struct vbdev_passthru, pt_bdev);

	spdk_json_write_object_begin(w);

	spdk_json_write_named_string(w, "method", "construct_passthru_bdev");

	spdk_json_write_named_object_begin(w, "params");
	//spdk_json_write_named_string(w, "base_bdev_name", spdk_bdev_get_name(pt_node->base_bdev));
	//spdk_json_write_named_string(w, "raid_bdev_name", spdk_bdev_get_name(bdev));
	spdk_json_write_object_end(w);

	spdk_json_write_object_end(w);
}

