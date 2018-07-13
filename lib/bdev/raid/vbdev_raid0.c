/*
 * vbdev_raid0.c
 *
 *  Created on: Jun 30, 2018
 *      Author: root
 */
#include "vbdev_common.h"

static int rdx_req_xfer0(struct rdx_req *req);
static int rdx_req_check_bitmap0(struct rdx_req *req,
				 struct rdx_stripe *stripe);
static void rdx_req_set_bitmap0(struct rdx_req *req,
				struct rdx_stripe *stripe, int val);
static void rdx_stt_configure0(struct rdx_raid_dsc *raid_dsc);
static void rdx_stt_configure_read0(rdx_stt *stt);
static void rdx_stt_configure_write0(rdx_stt *stt);

int rdx_raid0_dsc_configure(struct rdx_raid_dsc *raid_dsc)
{
	raid_dsc->strips_per_substripe = raid_dsc->dev_cnt;
	raid_dsc->substripes_cnt = 1;
	raid_dsc->synd_cnt = 0;
	raid_dsc->data_cnt = raid_dsc->dev_cnt;

	raid_dsc->req_check_bitmap = rdx_req_check_bitmap0;
	raid_dsc->req_set_bitmap = rdx_req_set_bitmap0;

	rdx_stt_configure0(raid_dsc);

	return 0;
}

static int rdx_req_xfer0(struct rdx_req *req)
{
//	rdx_req_get_ref(req);
//
//	rdx_req_xfer_data(req);
//
//	rdx_req_put_ref(req);

	return 0;
}

static int rdx_req_check_bitmap0(struct rdx_req *req,
				 struct rdx_stripe *stripe)
{
//	struct rdx_raid_dsc *raid_dsc = req->raid_dsc;
//
//	int start_bit;
//	int len_in_bits;
//
//	start_bit = (req->addr % raid_dsc->stripe_data_len) / RDX_BLOCK_SIZE_SECTORS;
//	len_in_bits = req->len / RDX_BLOCK_SIZE_SECTORS;
//
//	smp_rmb();
//
//	/* Calculate data bits */
//	if (!rdx_bitmap_area_is_clear(stripe->rw_bitmap, start_bit,
//					len_in_bits)) {
//		return -1;
//	}
//
//	req->start_bit = start_bit;
//	req->len_in_bits = len_in_bits;

	return 0;
}

static void rdx_req_set_bitmap0(struct rdx_req *req,
				struct rdx_stripe *stripe, int val)
{
//	if (val)
//		bitmap_set(stripe->rw_bitmap, req->start_bit,
//			   req->len_in_bits);
//	else
//		bitmap_clear(stripe->rw_bitmap, req->start_bit,
//			     req->len_in_bits);
//
//	smp_wmb();
}

static void rdx_stt_configure0(struct rdx_raid_dsc *raid_dsc)
{
#define STT raid_dsc->stt
	memset(STT, 0, sizeof(STT));
	rdx_stt_configure_read0(&STT[RDX_REQ_TYPE_READ]);
	rdx_stt_configure_write0(&STT[RDX_REQ_TYPE_WRITE]);
#undef STT
}

static void rdx_stt_configure_read0(rdx_stt *stt)
{
	(*stt)[RDX_REQ_STATE_CREATE][RDX_REQ_EVENT_CREATED].state = RDX_REQ_STATE_ASSIGN;
	(*stt)[RDX_REQ_STATE_CREATE][RDX_REQ_EVENT_CREATED].func = rdx_req_assign;

	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_ASSIGNED].state = RDX_REQ_STATE_READ;
	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_ASSIGNED].func = rdx_req_xfer0;
	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_BUSY].state = RDX_REQ_STATE_ASSIGN;
	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_BUSY].func = rdx_req_assign;

	(*stt)[RDX_REQ_STATE_READ][RDX_REQ_EVENT_XFER_COMPLETED].state = RDX_REQ_STATE_COMPLETE;
	(*stt)[RDX_REQ_STATE_READ][RDX_REQ_EVENT_XFER_COMPLETED].func = rdx_req_complete;
	(*stt)[RDX_REQ_STATE_READ][RDX_REQ_EVENT_XFER_FAILED].state = RDX_REQ_STATE_COMPLETE;
	(*stt)[RDX_REQ_STATE_READ][RDX_REQ_EVENT_XFER_FAILED].func = rdx_req_complete;

	(*stt)[RDX_REQ_STATE_FAIL][RDX_REQ_EVENT_FAILED].state = RDX_REQ_STATE_COMPLETE;
	(*stt)[RDX_REQ_STATE_FAIL][RDX_REQ_EVENT_FAILED].func = rdx_req_complete;

	(*stt)[RDX_REQ_STATE_COMPLETE][RDX_REQ_EVENT_COMPLETED].state = RDX_REQ_STATE_DESTROY;
	(*stt)[RDX_REQ_STATE_COMPLETE][RDX_REQ_EVENT_COMPLETED].func = rdx_req_destroy;
}

static void rdx_stt_configure_write0(rdx_stt *stt)
{
	(*stt)[RDX_REQ_STATE_CREATE][RDX_REQ_EVENT_CREATED].state = RDX_REQ_STATE_ASSIGN;
	(*stt)[RDX_REQ_STATE_CREATE][RDX_REQ_EVENT_CREATED].func = rdx_req_assign;

	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_ASSIGNED].state = RDX_REQ_STATE_WRITE;
	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_ASSIGNED].func = rdx_req_xfer0;
	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_BUSY].state = RDX_REQ_STATE_ASSIGN;
	(*stt)[RDX_REQ_STATE_ASSIGN][RDX_REQ_EVENT_BUSY].func = rdx_req_assign;

	(*stt)[RDX_REQ_STATE_WRITE][RDX_REQ_EVENT_XFER_COMPLETED].state = RDX_REQ_STATE_COMPLETE;
	(*stt)[RDX_REQ_STATE_WRITE][RDX_REQ_EVENT_XFER_COMPLETED].func = rdx_req_complete;
	(*stt)[RDX_REQ_STATE_WRITE][RDX_REQ_EVENT_XFER_FAILED].state = RDX_REQ_STATE_COMPLETE;
	(*stt)[RDX_REQ_STATE_WRITE][RDX_REQ_EVENT_XFER_FAILED].func = rdx_req_complete;

	(*stt)[RDX_REQ_STATE_FAIL][RDX_REQ_EVENT_FAILED].state = RDX_REQ_STATE_COMPLETE;
	(*stt)[RDX_REQ_STATE_FAIL][RDX_REQ_EVENT_FAILED].func = rdx_req_complete;

	(*stt)[RDX_REQ_STATE_COMPLETE][RDX_REQ_EVENT_COMPLETED].state = RDX_REQ_STATE_DESTROY;
	(*stt)[RDX_REQ_STATE_COMPLETE][RDX_REQ_EVENT_COMPLETED].func = rdx_req_destroy;
}
