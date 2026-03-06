// SPDX-License-Identifier: BSD-3-Clause

#include "ring_buffer.h"
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	if (!ring)
		return -1;
	ring->data = malloc(cap);
	if (!ring->data)
		return -1;
	ring->write_pos = 0;
	ring->read_pos = 0;
	ring->len = 0;
	ring->cap = cap;
	ring->stop = 0;
	pthread_mutex_init(&ring->mutex, NULL);
	pthread_cond_init(&ring->not_empty, NULL);
	pthread_cond_init(&ring->not_full, NULL);
	return cap;
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&ring->mutex);
	while (ring->len + size > ring->cap)
		pthread_cond_wait(&ring->not_full, &ring->mutex);

	size_t space_at_end = ring->cap - ring->write_pos;
	size_t chunk1 = (size < space_at_end) ? size : space_at_end;

	memcpy(ring->data + ring->write_pos, data, chunk1);

	if (size > chunk1)
		memcpy(ring->data, (char *)data + chunk1, size - chunk1);

	ring->write_pos = (ring->write_pos + size) % ring->cap;
	ring->len += size;
	pthread_cond_signal(&ring->not_empty);
	pthread_mutex_unlock(&ring->mutex);
	return size;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size)
{
	pthread_mutex_lock(&ring->mutex);
	while (ring->len < size && !ring->stop)
		pthread_cond_wait(&ring->not_empty, &ring->mutex);

	if (ring->stop && ring->len < size) {
		if (ring->len == 0) {
			pthread_mutex_unlock(&ring->mutex);
			return 0;
		}
		size = ring->len;
	}

	size_t available_at_end = ring->cap - ring->read_pos;
	size_t part1 = (size < available_at_end) ? size : available_at_end;

	memcpy(data, ring->data + ring->read_pos, part1);

	if (size > part1)
		memcpy((char *)data + part1, ring->data, size - part1);

	ring->read_pos = (ring->read_pos + size) % ring->cap;
	ring->len -= size;
	pthread_cond_signal(&ring->not_full);
	pthread_mutex_unlock(&ring->mutex);
	return size;
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	if (!ring)
		return;
	free(ring->data);
	pthread_mutex_destroy(&ring->mutex);
	pthread_cond_destroy(&ring->not_empty);
	pthread_cond_destroy(&ring->not_full);
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	pthread_mutex_lock(&ring->mutex);
	ring->stop = 1;
	pthread_cond_broadcast(&ring->not_empty);
	pthread_mutex_unlock(&ring->mutex);
}
