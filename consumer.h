/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __SO_CONSUMER_H__
#define __SO_CONSUMER_H__

#include <pthread.h>
#include "ring_buffer.h"
#include "packet.h"

#define OUT_BUF_SZ 4096
#define BATCH_SZ 16

typedef struct {
	unsigned long timestamp;
	char log_line[64];
	int len;
} pkt_info_t;

typedef struct {
	pkt_info_t *heap;
	size_t size;
	size_t capacity;

	int out_fd;
	int active_threads;

	char out_buffer[OUT_BUF_SZ];
	int out_buf_pos;

	pthread_mutex_t lock;
} so_consum_monitor_t;

typedef struct so_consumer_ctx_t {
	struct so_ring_buffer_t *producer_rb;
	so_consum_monitor_t *monitor;
} so_consumer_ctx_t;

int create_consumers(pthread_t *tids,
					int num_consumers,
					so_ring_buffer_t *rb,
					const char *out_filename);

#endif /* __SO_CONSUMER_H__ */
