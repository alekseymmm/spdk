/*
 * vbdev_raid0.c
 *
 *  Created on: Jun 30, 2018
 *      Author: root
 */
#include "vbdev_common.h"

int rdx_raid0_dsc_configure(struct rdx_raid_dsc *raid_dsc)
{
	raid_dsc->strips_per_substripe = raid_dsc->dev_cnt;
	raid_dsc->substripes_cnt = 1;
	raid_dsc->synd_cnt = 0;
	raid_dsc->data_cnt = raid_dsc->dev_cnt;

//	raid_dsc->req_check_bitmap = rdx_req_check_bitmap0;
//	raid_dsc->req_set_bitmap = rdx_req_set_bitmap0;
//
//	rdx_stt_configure0(raid_dsc);

	return 0;
}
