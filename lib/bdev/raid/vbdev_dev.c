/*
 * vbdev_dev.c
 *
 *  Created on: 21 мая 2018 г.
 *      Author: root
 */
#include "spdk/bdev_module.h"
#include "vbdev_common.h"
#include "vbdev_dev.h"

void rdx_dev_close(struct rdx_dev *dev);
void rdx_dev_destroy(struct rdx_dev *dev);
int rdx_dev_register(struct rdx_dev *dev, struct spdk_bdev *bdev);

/* Called when the underlying base bdev goes away. */
static void
vbdev_raid_base_bdev_hotremove_cb(void *ctx)
{
	struct rdx_dev *dev = (struct rdx_dev *)ctx;
	struct rdx_raid *raid = dev->raid;
	/*
	 *unregister dev in raid and do  spdk_bdev_unregister
	 */

	rdx_dev_destroy(dev);
	spdk_io_device_unregister(raid, NULL);
	spdk_bdev_unregister(&raid->raid_bdev, NULL, NULL);

//	TAILQ_FOREACH_SAFE(pt_node, &g_pt_nodes, link, tmp) {
//		if (bdev_find == pt_node->base_bdev) {
//			spdk_bdev_unregister(&pt_node->pt_bdev, NULL, NULL);
//		}
//	}
}

int rdx_dev_register(struct rdx_dev *dev, struct spdk_bdev *bdev)
{
	int rc;
	uint64_t block_size_sectors;
	struct rdx_raid *raid = dev->raid;

	rc = spdk_bdev_open(bdev, true, vbdev_raid_base_bdev_hotremove_cb,
			dev, &dev->base_desc);
	if (rc) {
		SPDK_ERRLOG("could not open bdev %s\n", dev->bdev_name);
		return -1;
	}
	SPDK_NOTICELOG("bdev %s opened in raid module\n", bdev->name);

	dev->bdev = bdev;

	rc = spdk_bdev_module_claim_bdev(bdev, dev->base_desc, dev->raid->module);

	if (rc) {
		SPDK_ERRLOG("could not open bdev %s\n", spdk_bdev_get_name(bdev));
		spdk_bdev_close(dev->base_desc);
		goto error;
	}
	SPDK_NOTICELOG("bdev %s claimed by raid module\n", bdev->name);

	block_size_sectors = spdk_bdev_get_block_size(dev->bdev) / KERNEL_SECTOR_SIZE;
	dev->size = spdk_bdev_get_num_blocks(dev->bdev) *
			block_size_sectors - RDX_MD_OFFSET;
	//TODO: do it the same way as in raidix_nvme, not like this!
	if (!raid->size) { //create raid
		if (dev->size < raid->dev_size || raid->dev_size == 0) {
			raid->dev_size = dev->size;
		}
	} else { // restore raid
		//to be done
	}

	return 0;
error:
	return -1;
}

int rdx_dev_create(struct rdx_raid *raid, struct rdx_devices *devices,
		int dev_num)
{
	struct rdx_dev *dev;
	struct spdk_bdev *bdev;
//	uint64_t strips_in_dev;
	int rc = 0;

	dev = calloc(1, sizeof(struct rdx_dev));
	if (!dev) {
		SPDK_ERRLOG("Cannot allocate memory for dev\n");
		return -ENOMEM;
	}

//	INIT_HLIST_NODE(&dev->hlist);
//	init_waitqueue_head(&dev->wait);
	dev->num = dev_num;
	dev->raid = raid;
	raid->devices[dev_num] = dev;

//	dev->md = (struct rdx_md *)get_zeroed_page(GFP_KERNEL);
//	if (!dev->md) {
//		SPDK_ERRLOG("Cannot allocate page for metadata\n");
//		goto error;
//	}

//	dev->recon_md = (struct rdx_recon_md *)get_zeroed_page(GFP_KERNEL);
//	if (!dev->recon_md) {
//		SPDK_ERRLOG("Cannot allocate page for reconstruction metadata\n");
//		goto error;
//	}

	if (rdx_dev_is_null(devices->names[dev_num])) {
		SPDK_NOTICELOG("Device null number %d added to raid %s\n",
			 dev->num, raid->name);
		//rdx_dev_clear_state(dev, RDX_DEV_STATE_ONLINE);
		return 0;
	}

	/* Start opening device */
	//atomic_set(&dev->ref_cnt, 1);

	dev->bdev_name = strdup(devices->names[dev_num]);
	if (!dev->bdev_name) {
		SPDK_ERRLOG("Cannot allocate memory for bdev_name\n");
		goto error;
	}

	bdev = spdk_bdev_get_by_name(dev->bdev_name);
	if(!bdev) {
		SPDK_NOTICELOG("Cannot get bdev=%s by name\n", dev->bdev_name);
		dev->bdev = NULL;
		//rdx_dev_clear_state(dev, RDX_DEV_STATE_ONLINE);
		return 0;
	}

	rc = rdx_dev_register(dev, bdev);
	if (!rc) {
		goto error;
	}

	SPDK_NOTICELOG("Device %s number %d of size %lu sectors added to raid %s\n",
		 dev->bdev_name, dev->num, dev->size, raid->name);

	//rdx_dev_update(dev);

	return 0;

error:
	rdx_dev_destroy(dev);
	return -ENOMEM;
}

void rdx_dev_close(struct rdx_dev *dev)
{
//	rdx_dev_clear_state(dev, RDX_DEV_STATE_ONLINE);
//
//	/* Dev is closed */
//	if (!atomic_read(&dev->ref_cnt))
//		return;

	SPDK_NOTICELOG("Closing dev %s number %d\n", dev->bdev_name, dev->num);

//	rdx_dev_put_ref(dev);
//	wait_event(dev->wait, !atomic_read(&dev->ref_cnt));

	/* Remove bdev from device. Do not delete the rdx_dev structure */
	if (dev->bdev){
		spdk_bdev_module_release_bdev(dev->bdev);
		if (dev->bdev->status == SPDK_BDEV_STATUS_READY)
			spdk_bdev_close(dev->base_desc);
	}
	if (dev->bdev_name)
		free(dev->bdev_name);
	dev->bdev = NULL;
	dev->bdev_name = NULL;

	SPDK_NOTICELOG("Closed dev number %d\n", dev->num);
}

void rdx_dev_destroy(struct rdx_dev *dev)
{
	struct rdx_raid *raid = dev->raid;
	raid->devices[dev->num] = NULL;
	rdx_dev_close(dev);
//	if (dev->recon_md)
//		free_page((unsigned long)dev->recon_md);
//	if (dev->md)
//		free_page((unsigned long)dev->md);
	free(dev);
}

