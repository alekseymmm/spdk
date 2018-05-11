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

#include "spdk_internal/bdev.h"
#include "spdk_internal/log.h"

static int vbdev_raid_init(void);
static void vbdev_raid_get_spdk_running_config(FILE *fp);
static int vbdev_raid_get_ctx_size(void);
static void vbdev_raid_examine(struct spdk_bdev *bdev);
static void vbdev_raid_finish(void);

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


/* When we regsiter our bdev this is how we specify our entry points. */
static const struct spdk_bdev_fn_table vbdev_raid_fn_table = {
	.destruct		= NULL,//vbdev_passthru_destruct,
	.submit_request		= NULL,//vbdev_passthru_submit_request,
	.io_type_supported	= NULL,//vbdev_passthru_io_type_supported,
	.get_io_channel		= NULL,//vbdev_passthru_get_io_channel,
	.dump_info_json		= NULL,//vbdev_passthru_info_config_json,
	.write_config_json	= vbdev_raid_write_json_config,//
};

static struct spdk_bdev_module raid_if = {
	.name = "raid",
	.module_init = vbdev_raid_init,
	.config_text = NULL,//vbdev_raid_get_spdk_running_config,
	.get_ctx_size = NULL,//vbdev_raid_get_ctx_size,
	.examine = NULL,//vbdev_raid_examine,
	.module_fini = NULL,//vbdev_raid_finish
};

SPDK_BDEV_MODULE_REGISTER(&raid_if)

static int vbdev_raid_init(void)
{
	struct spdk_conf_section *sp = NULL;
	char *conf_bdev_name = NULL;
	char *conf_vbdev_name = NULL;
	int i, rc;
	int drives_num, stripe_size_kb;

	sp = spdk_conf_find_section(NULL, "RAID");
	if (sp == NULL) {
		return 0;
	}

	if (sp != NULL){
		conf_vbdev_name = spdk_conf_section_get_val(sp, "RAIDName");
		if (!conf_vbdev_name) {
			SPDK_ERRLOG("RAID configuration missing raid_bdev name\n");
			break;
		}
		drives_num = spdk_conf_section_get_intval(sp, "NumberOfDrives");
		stripe_size_kb = spdk_conf_section_get_intval(sp, "StripeSizeKB");

		for (i = 0 ; i < drives_num; i++) {
			conf_bdev_name = spdk_conf_section_get_nmval(sp,
					"Drive", i, 0);
		}
	}


	for (i = 0; ; i++) {
		conf_bdev_name = spdk_conf_section_get_nmval(sp, "rdx", i, 1);
		if (!conf_bdev_name) {
			SPDK_ERRLOG("RAID configuration missing bdev0 name\n");
			break;
		}

//		rc = vbdev_passthru_insert_name(conf_bdev_name, conf_vbdev_name);
//		if (rc != 0) {
//			return rc;
//		}
	}
//	TAILQ_FOREACH(name, &g_bdev_names, link) {
//		SPDK_NOTICELOG("conf parse matched: %s\n", name->bdev_name);
//	}
	return 0;
}

int create_raid_disk(const char *bdev_name, const char *vbdev_name)
{
	return 0;
}
