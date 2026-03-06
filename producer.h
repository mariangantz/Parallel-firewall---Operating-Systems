/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __SO_PRODUCER_H__
#define __SO_PRODUCER_H__

#include "ring_buffer.h"
#include "packet.h"
extern pthread_mutex_t MUTEX_LOG;

void publish_data(struct so_ring_buffer_t *rb, const char *filename);

#endif /*__SO_PRODUCER_H__*/
