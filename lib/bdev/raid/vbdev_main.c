/*
 * vbdev_raid.c
 *
 *  Created on: 10 мая 2018 г.
 *      Author: alekseym
 */


#include "spdk/stdinc.h"

//#include "vbdev_raid.h"
#include "spdk/rpc.h"
#include "spdk/env.h"
#include "spdk/conf.h"
#include "spdk/endian.h"
#include "spdk/string.h"
#include "spdk/io_channel.h"
#include "spdk/util.h"

#include "spdk/bdev_module.h"
#include "spdk_internal/log.h"

#include "vbdev_common.h"
#include "vbdev_raid.h"

struct rdx_raid *g_raid;

static int vbdev_raid_init(void);
//static void vbdev_raid_get_spdk_running_config(FILE *fp);
//static int vbdev_raid_get_ctx_size(void);
static void vbdev_raid_examine(struct spdk_bdev *bdev);
static void vbdev_raid_init_complete(void);
static void vbdev_raid_finish(void);

struct spdk_bdev_module raid_if = {
	.name = "raid",
	.module_init = vbdev_raid_init,
	.init_complete = vbdev_raid_init_complete,
	.config_text = NULL,//vbdev_raid_get_spdk_running_config,
	.get_ctx_size = NULL,//vbdev_raid_get_ctx_size,
	.examine_config = vbdev_raid_examine,
	.module_fini = vbdev_raid_finish,
};

SPDK_BDEV_MODULE_REGISTER(&raid_if);

static int vbdev_raid_init(void)
{
	struct spdk_conf_section *sp = NULL;
	char *conf_bdev_name = NULL;
	char *conf_vbdev_name = NULL;
	struct rdx_devices devices;
	int i, ret = 0;
	int drives_num, stripe_size_kb, level;

	sp = spdk_conf_find_section(NULL, "RAID");
	if (sp == NULL) {
		return 0;
	}

	if (sp != NULL){
		conf_vbdev_name = spdk_conf_section_get_val(sp, "RAIDName");
		if (!conf_vbdev_name) {
			SPDK_ERRLOG("RAID configuration missing raid_bdev name\n");
		}
		drives_num = spdk_conf_section_get_intval(sp, "NumberOfDrives");
		stripe_size_kb = spdk_conf_section_get_intval(sp, "StripeSizeKB");
		level = spdk_conf_section_get_intval(sp, "Level");

		devices.names = calloc(1, sizeof(char *) * drives_num);
		devices.cnt = 0;
		for (i = 0 ; i < drives_num; i++) {
			conf_bdev_name = spdk_conf_section_get_nmval(sp,
					"Drive", i, 0);
			devices.names[i] = strdup(conf_bdev_name);
			devices.cnt++;
		}

		ret = spdk_raid_create(conf_vbdev_name, level, stripe_size_kb,
				&devices, 0);
		if (!ret) {

		}
		SPDK_NOTICELOG("RAID %s created.\n",
				conf_vbdev_name);

		free(devices.names);
	}


//		rc = vbdev_passthru_insert_name(conf_bdev_name, conf_vbdev_name);
//		if (rc != 0) {
//			return rc;
//		}
//	TAILQ_FOREACH(name, &g_bdev_names, link) {
//		SPDK_NOTICELOG("conf parse matched: %s\n", name->bdev_name);
//	}
	return 0;
}

int create_raid_disk(const char *bdev_name, const char *vbdev_name)
{
	return 0;
}

static void vbdev_raid_examine(struct spdk_bdev *bdev)
{
	int i, ret;
	struct rdx_raid_dsc *raid_dsc;

	if (g_raid)
	{
		raid_dsc = g_raid->dsc;
		for (i = 0; i < raid_dsc->dev_cnt; i++) {
			if (strcmp(raid_dsc->devices[i]->bdev_name, bdev->name) != 0) {
				continue;
			}

			ret = rdx_raid_replace(raid_dsc, i, bdev);
			if (ret) {
				SPDK_ERRLOG("Failed to replace device %s on"
					    " position %d\n", bdev->name, i);
			}
			break;
		}
	}
	spdk_bdev_module_examine_done(&raid_if);
}

static void vbdev_raid_init_complete(void)
{
	if (g_raid) {
		rdx_raid_register(g_raid);
	}
}

static void vbdev_raid_finish(void)
{
	//spdk_bdev_unregister(&g_raid->raid_bdev, NULL, NULL);

	//rdx_raid_destroy_devices(&g_raid);
	printf("raid fini called\n");
	return;
}

SPDK_LOG_REGISTER_COMPONENT("vbdev_raid", SPDK_LOG_VBDEV_RAID);
