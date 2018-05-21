/*
 * vbdev_raid_rpc.c
 *
 *  Created on: 10 мая 2018 г.
 *      Author: alekseym
 */

#include "vbdev_raid.h"
#include "spdk/rpc.h"
#include "spdk/util.h"

#include "spdk_internal/log.h"

/* Structure to hold the parameters for this RPC method. */
struct rpc_construct_raid {
	char *base_bdev_name;
	char *raid_bdev_name;
};

/* Free the allocated memory resource after the RPC handling. */
static void free_rpc_construct_raid(struct rpc_construct_raid *r)
{
	free(r->base_bdev_name);
	free(r->raid_bdev_name);
}

/* Structure to decode the input parameters for this RPC method. */
static const struct spdk_json_object_decoder rpc_construct_raid_decoders[] = {
	{"base_bdev_name",
	 offsetof(struct rpc_construct_raid, base_bdev_name),
	 spdk_json_decode_string},
	{"raid_bdev_name",
	 offsetof(struct rpc_construct_raid, raid_bdev_name),
	 spdk_json_decode_string},
};


/* Decode the parameters for this RPC method and properly construct the raid
 * device. Error status returned in the failed cases.
 */
static void spdk_rpc_construct_raid_bdev(
				struct spdk_jsonrpc_request *request,
				const struct spdk_json_val *params)
{
	struct rpc_construct_raid req = {NULL};
	struct spdk_json_write_ctx *w;
	int rc;

	if (spdk_json_decode_object(params, rpc_construct_raid_decoders,
				    SPDK_COUNTOF(rpc_construct_raid_decoders),
				    &req)) {
		SPDK_DEBUGLOG(SPDK_LOG_VBDEV_PASSTHRU,
				"spdk_json_decode_object failed\n");
		goto invalid;
	}

//	rc = create_raid_disk(req.base_bdev_name, req.raid_bdev_name);
//	if (rc != 0) {
//		goto invalid;
//	}

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		free_rpc_construct_raid(&req);
		return;
	}

	spdk_json_write_array_begin(w);
	spdk_json_write_string(w, req.raid_bdev_name);
	spdk_json_write_array_end(w);
	spdk_jsonrpc_end_result(request, w);
	free_rpc_construct_raid(&req);
	return;

invalid:
	free_rpc_construct_raid(&req);
	spdk_jsonrpc_send_error_response(request,
			SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid parameters");
}
SPDK_RPC_REGISTER("construct_raid_bdev", spdk_rpc_construct_raid_bdev, SPDK_RPC_RUNTIME)

