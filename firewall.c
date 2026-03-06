// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/types.h>

#include "utils.h"
#include "ring_buffer.h"
#include "consumer.h"
#include "producer.h"
#include "packet.h"
#include "log/log.h"

#define SO_RING_SZ		(PKT_SZ * 1000)

void log_lock(bool lock, void *udata)
{
	pthread_mutex_t *LOCK = (pthread_mutex_t *) udata;

	if (!lock)
		pthread_mutex_unlock(LOCK);
	else
		pthread_mutex_lock(LOCK);
}

void __attribute__((constructor)) init() {
	log_set_lock(log_lock, &MUTEX_LOG);
	pthread_mutex_init(&MUTEX_LOG, NULL);
}

void __attribute__((destructor)) dest() {
	pthread_mutex_destroy(&MUTEX_LOG);
}

pthread_mutex_t MUTEX_LOG;

int main(int argc, char **argv)
{
	int num_consumers, threads, rc;
	pthread_t *thread_ids = NULL;
	so_ring_buffer_t ring_buffer;

	void check_args(int argc, char *progname)
{
	if (argc < 4) {
		fprintf(stderr, "Usage: %s <input-file> <output-file> <num-consumers:1-32>\n", progname);
		exit(EXIT_FAILURE);
	}
}

	check_args(argc, argv[0]);

	rc = ring_buffer_init(&ring_buffer, SO_RING_SZ);
	if (rc < 0) {
		fprintf(stderr, "Error: ring_buffer_init failed (rc = %d)\n", rc);
		exit(EXIT_FAILURE);
}

	num_consumers = strtol(argv[3], NULL, 10);
	if (num_consumers >= 1 && num_consumers <= 32) {
		;
	} else {
		fprintf(stderr, "num-consumers [%d] must be in the interval [1-32]\n", num_consumers);
		exit(EXIT_FAILURE);
}


	thread_ids = malloc(num_consumers * sizeof(pthread_t));
	if (thread_ids == NULL) {
		perror("malloc pthread_t");
		exit(EXIT_FAILURE);
	}

	char *file_in = argv[1];
	char *config = argv[2];

	threads = create_consumers(thread_ids, num_consumers, &ring_buffer, config);
	publish_data(&ring_buffer, file_in);


	int i = 0;

	while (i < num_consumers) {
		pthread_join(thread_ids[i], NULL);
		i++;
	}


	ring_buffer_destroy(&ring_buffer);
	free(thread_ids);

	return threads;
}
