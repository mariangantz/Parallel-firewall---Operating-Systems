// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "consumer.h"
#include "ring_buffer.h"
#include "packet.h"
#include "utils.h"

static void flush_buffer(so_consum_monitor_t *mon)
{
	if (mon->out_buf_pos > 0) {
		int written = 0;

		while (written < mon->out_buf_pos) {
			int ret = write(mon->out_fd,
							mon->out_buffer + written,
							mon->out_buf_pos - written);
			if (ret < 0)
				break;
			written += ret;
		}
		mon->out_buf_pos = 0;
	}
}

static void buffer_write(so_consum_monitor_t *mon, const char *data, int len)
{
	if (mon->out_buf_pos + len > OUT_BUF_SZ)
		flush_buffer(mon);
	memcpy(mon->out_buffer + mon->out_buf_pos, data, len);
	mon->out_buf_pos += len;
}

static void swap_pkt(pkt_info_t *a, pkt_info_t *b)
{
	pkt_info_t temp = *a;
	*a = *b;
	*b = temp;
}

static void heap_push(so_consum_monitor_t *mon, pkt_info_t info)
{
	size_t pos = mon->size++;

	mon->heap[pos] = info;

	while (pos > 0) {
		size_t parent = (pos - 1) / 2;

		if (mon->heap[pos].timestamp < mon->heap[parent].timestamp) {
			swap_pkt(&mon->heap[pos], &mon->heap[parent]);
			pos = parent;
		} else {
			break;
		}
	}
}

static pkt_info_t heap_pop(so_consum_monitor_t *mon)
{
	pkt_info_t min_pkt = mon->heap[0];

	mon->size--;
	mon->heap[0] = mon->heap[mon->size];

	size_t pos = 0;

	while (pos < mon->size / 2) {
		size_t left = 2 * pos + 1;
		size_t right = left + 1;
		size_t smallest = (right < mon->size &&
			mon->heap[right].timestamp < mon->heap[left].timestamp) ? right : left;
		if (mon->heap[smallest].timestamp < mon->heap[pos].timestamp) {
			swap_pkt(&mon->heap[pos], &mon->heap[smallest]);
			pos = smallest;
		} else {
			break;
		}
	}
	return min_pkt;
}

void *consumer_thread(void *ctx)
{
	so_consumer_ctx_t *cctx = (so_consumer_ctx_t *)ctx;
	so_ring_buffer_t *rb = cctx->producer_rb;
	so_consum_monitor_t *mon = cctx->monitor;

	char buffer[PKT_SZ * BATCH_SZ];
	pkt_info_t batch_info[BATCH_SZ];

	ssize_t bytes_read;

	while ((bytes_read = ring_buffer_dequeue(rb, buffer, PKT_SZ * BATCH_SZ)) > 0) {
		int num_pkts = bytes_read / PKT_SZ;

		for (int i = 0; i < num_pkts; i++) {
			struct so_packet_t *pkt = (struct so_packet_t *)(buffer + (i * PKT_SZ));

			unsigned long action = process_packet(pkt);
			unsigned long hash = packet_hash(pkt);
			unsigned long timestamp = pkt->hdr.timestamp;

			batch_info[i].timestamp = timestamp;
			batch_info[i].len = snprintf(batch_info[i].log_line,
										 sizeof(batch_info[i].log_line),
										 "%s %016lx %lu\n",
										 RES_TO_STR(action), hash, timestamp);
		}

		pthread_mutex_lock(&mon->lock);

		for (int i = 0; i < num_pkts; i++)
			heap_push(mon, batch_info[i]);

		while (mon->size > mon->capacity) {
			pkt_info_t out = heap_pop(mon);

			buffer_write(mon, out.log_line, out.len);
		}

		pthread_mutex_unlock(&mon->lock);
	}

	pthread_mutex_lock(&mon->lock);
	mon->active_threads--;
	if (mon->active_threads == 0) {
		while (mon->size > 0) {
			pkt_info_t out = heap_pop(mon);

			buffer_write(mon, out.log_line, out.len);
		}
		flush_buffer(mon);
	}
	pthread_mutex_unlock(&mon->lock);

	return NULL;
}

int create_consumers(pthread_t *tids, int num_consumers,
					 so_ring_buffer_t *rb, const char *out_filename)
{
	int fd = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (fd < 0)
		return -1;

	so_consum_monitor_t *mon = malloc(sizeof(so_consum_monitor_t));

	mon->capacity = (size_t)(num_consumers * BATCH_SZ);
	mon->heap = malloc(sizeof(pkt_info_t) * (mon->capacity * 4 + 1024));

	mon->size = 0;
	mon->out_fd = fd;
	mon->active_threads = num_consumers;
	mon->out_buf_pos = 0;
	pthread_mutex_init(&mon->lock, NULL);

	so_consumer_ctx_t *ctx_array = malloc(num_consumers * sizeof(so_consumer_ctx_t));

	for (int i = 0; i < num_consumers; i++) {
		ctx_array[i].producer_rb = rb;
		ctx_array[i].monitor = mon;
		pthread_create(&tids[i], NULL, consumer_thread, &ctx_array[i]);
	}
	return num_consumers;
}
