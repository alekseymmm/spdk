/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/string.h"
#include "spdk/bdev_module.h"

static char *g_bdev_name = "Malloc0";
static char *g_buff = NULL;
static uint64_t g_block_offset = 0;
static uint32_t g_blocks_cnt = 1;

/*
 * We'll use this struct to gather housekeeping hello_context to pass between
 * our events and callbacks.
 */
struct hello_context_t {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;
	char *buff;
	char *bdev_name;
	uint64_t block_offset;
	uint32_t block_cnt;
};

/*
 * Usage function for printing parameters that are specific to this application
 */
static void
hello_bdev_usage(void)
{
	printf(" -b bdev name\n");
}

/*
 * This function is called to parse the parameters that are specific to this application
 */
static void hello_bdev_parse_arg(int ch, char *arg)
{
	switch (ch) {
	case 'b':
		g_bdev_name = arg;
		break;
	case 'a':
		g_block_offset = atoll(arg);
		break;
	case 'l':
		g_blocks_cnt = atoi(arg);
		break;
	}
}

/*
 * Callback function for read io completion.
 */
static void
read_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct hello_context_t *hello_context = cb_arg;
	uint64_t blk_size;

	if (success) {
		//SPDK_NOTICELOG("Read string from bdev : %s\n", hello_context->buff);
		blk_size = spdk_bdev_get_block_size(hello_context->bdev) *
				hello_context->block_cnt;
		if (memcmp(hello_context->buff, g_buff, blk_size)) {
			SPDK_ERRLOG("read  write data mismatch.\n");
		} else {
			SPDK_NOTICELOG("Read data correct.\n");
		}
	} else {
		SPDK_ERRLOG("bdev io read error\n");
	}

	free(g_buff);

	/* Complete the bdev io and close the channel */
	spdk_bdev_free_io(bdev_io);
	spdk_put_io_channel(hello_context->bdev_io_channel);
	spdk_bdev_close(hello_context->bdev_desc);
	SPDK_NOTICELOG("Stopping app\n");
	spdk_app_stop(success ? 0 : -1);
}

/*
 * Callback function for write io completion.
 */
static void
write_complete(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
	struct hello_context_t *hello_context = cb_arg;
	int rc;
	uint32_t blk_size;
	uint64_t offset_bytes;

	/* Complete the I/O */
	spdk_bdev_free_io(bdev_io);

	if (success) {
		SPDK_NOTICELOG("bdev io write completed successfully\n");
	} else {
		SPDK_ERRLOG("bdev io write error: %d\n", EIO);
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	/* Zero the buffer so that we can use it for reading */
	blk_size = spdk_bdev_get_block_size(hello_context->bdev) *
			hello_context->block_cnt;
	offset_bytes = spdk_bdev_get_block_size(hello_context->bdev) *
			hello_context->block_offset;
	g_buff = calloc(1, blk_size);
	if (!g_buff) {
		SPDK_ERRLOG("Cannot allocate memory\n");
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	memcpy(g_buff, hello_context->buff, blk_size);
	memset(hello_context->buff, 0, blk_size);

	SPDK_NOTICELOG("Reading io\n");
	rc = spdk_bdev_read(hello_context->bdev_desc, hello_context->bdev_io_channel,
			    hello_context->buff, offset_bytes,
			    blk_size, read_complete, hello_context);

	if (rc) {
		SPDK_ERRLOG("%s error while reading from bdev: %d\n", spdk_strerror(-rc), rc);
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}
}

/*
 * Our initial event that kicks off everything from main().
 */
static void
hello_start(void *arg1, void *arg2)
{
	struct hello_context_t *hello_context = arg1;
	uint32_t blk_size, buf_align;
	uint64_t offset_bytes;
	int rc = 0;
	hello_context->bdev = NULL;
	hello_context->bdev_desc = NULL;
	char *test_string = "Hello World!\n";
	uint32_t written = 0;

	SPDK_NOTICELOG("Successfully started the application\n");

	/*
	 * Get the bdev. There can be many bdevs configured in
	 * in the configuration file but this application will only
	 * use the one input by the user at runtime so we get it via its name.
	 */
	hello_context->bdev = spdk_bdev_get_by_name(hello_context->bdev_name);
	if (hello_context->bdev == NULL) {
		SPDK_ERRLOG("Could not find the bdev: %s\n", hello_context->bdev_name);
		spdk_app_stop(-1);
		return;
	}

	/*
	 * Open the bdev by calling spdk_bdev_open()
	 * The function will return a descriptor
	 */
	SPDK_NOTICELOG("Opening the bdev %s\n", hello_context->bdev_name);
	rc = spdk_bdev_open(hello_context->bdev, true, NULL, NULL, &hello_context->bdev_desc);
	if (rc) {
		SPDK_ERRLOG("Could not open bdev: %s\n", hello_context->bdev_name);
		spdk_app_stop(-1);
		return;
	}

	SPDK_NOTICELOG("Opening io channel\n");
	/* Open I/O channel */
	hello_context->bdev_io_channel = spdk_bdev_get_io_channel(hello_context->bdev_desc);
	if (hello_context->bdev_io_channel == NULL) {
		SPDK_ERRLOG("Could not create bdev I/O channel!!\n");
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	/* Allocate memory for the write buffer.
	 * Initialize the write buffer with the string "Hello World!"
	 */
	blk_size = spdk_bdev_get_block_size(hello_context->bdev) *
			hello_context->block_cnt;
	offset_bytes = spdk_bdev_get_block_size(hello_context->bdev) *
			hello_context->block_offset;
	buf_align = spdk_bdev_get_buf_align(hello_context->bdev);
	hello_context->buff = spdk_dma_zmalloc(blk_size, buf_align, NULL);
	if (!hello_context->buff) {
		SPDK_ERRLOG("Failed to allocate buffer\n");
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_app_stop(-1);
		return;
	}

	while (written < blk_size) {
		written += snprintf(hello_context->buff + written,
				blk_size - written, "%s", test_string);
	}

	SPDK_NOTICELOG("Writing to the bdev\n");
	rc = spdk_bdev_write(hello_context->bdev_desc, hello_context->bdev_io_channel,
			     hello_context->buff, offset_bytes,
			     blk_size, write_complete, hello_context);
	if (rc) {
		SPDK_ERRLOG("%s error while writing to bdev: %d\n", spdk_strerror(-rc), rc);
		spdk_bdev_close(hello_context->bdev_desc);
		spdk_put_io_channel(hello_context->bdev_io_channel);
		spdk_app_stop(-1);
		return;
	}
}

int
main(int argc, char **argv)
{
	struct spdk_app_opts opts = {};
	int rc = 0;
	struct hello_context_t hello_context = {};

	/* Set default values in opts structure. */
	spdk_app_opts_init(&opts);
	opts.name = "hello_bdev";
	opts.config_file = "bdev.conf";

	/*
	 * The user can provide the config file and bdev name at run time.
	 * For example, to use Malloc0 in file bdev.conf run with params
	 * ./hello_bdev -c bdev.conf -b Malloc0
	 * To use passthru bdev PT0 run with params
	 * ./hello_bdev -c bdev.conf -b PT0
	 * If none of the parameters are provide the application will use the
	 * default parameters(-c bdev.conf -b Malloc0).
	 */
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "a:b:l:", hello_bdev_parse_arg,
				      hello_bdev_usage)) != SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}
	hello_context.bdev_name = g_bdev_name;
	hello_context.block_offset = g_block_offset;
	hello_context.block_cnt = g_blocks_cnt;

	/*
	 * spdk_app_start() will block running hello_start() until
	 * spdk_app_stop() is called by someone (not simply when
	 * hello_start() returns), or if an error occurs during
	 * spdk_app_start() before hello_start() runs.
	 */
	rc = spdk_app_start(&opts, hello_start, &hello_context, NULL);
	if (rc) {
		SPDK_ERRLOG("ERROR starting application\n");
	}

	/* When the app stops, free up memory that we allocated. */
	spdk_dma_free(hello_context.buff);

	/* Gracefully close out all of the SPDK subsystems. */
	spdk_app_fini();
	return rc;
}
