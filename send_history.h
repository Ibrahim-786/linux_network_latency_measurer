/*
 * network latency measurer
 * Copyright (C) 2018  Ricardo Biehl Pasquali <pasqualirb@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * 03/05/2018
 *
 * buffer where send timestamps are stored
 *
 * if a new entry is request and the buffer is full, the
 * timeout of the oldest (next) entry has elapsed and we
 * can overwrite it. See setup_send_history() in main.c
 */

#ifndef SEND_HISTORY_H
#define SEND_HISTORY_H

#include <stdint.h> /* uint64_t */
#include <pthread.h> /* pthread_mutex_t */

/*
 * single ring buffer
 *
 * From the implementation in deviation_history.h
 * of ipmic synchronization.
 * It's not a producer/consumer ring buffer!
 */

struct single_ring_buffer {
	int is_full;
	unsigned int current;
	unsigned int size;
};

#define single_ring_buffer_reset(b) \
do { \
	(b)->is_full = 0; \
	(b)->current = 0; \
} while (0)

//#define single_ring_buffer_get_last(b) ((b)->is_full ? (b)->current : 0)

static inline void
single_ring_buffer_update(struct single_ring_buffer *b)
{
	b->current++;
	if (b->current == b->size) {
		b->current = 0;

		/*
		 * once the buffer gets full it is always
		 * full until be reset
		 */
		if (!b->is_full)
			b->is_full = 1;
	}
}

/* ---------------------------------------- */

struct sent_packet {
	uint64_t id;
	uint64_t flags;

	/* userspace timestamp at the time of send() call */
	struct timespec userspace_ts;
	/* kernel timestamp when packet was sent */
	struct timespec ts;
#ifdef WRITE_IN_SENDER
	struct timespec recv_ts;
#endif
};

#define PACKET_ID_MASK  0x000000ffffffffff
#define PACKET_ID_MAX   0x000000ffffffffff

/* the history of sent packets */
struct send_history {
	uint64_t packet_id_boundary;

	/* controls access to send history */
	pthread_mutex_t mtx;

	struct sent_packet *buffer;
	struct single_ring_buffer control;
};

/* values in flags */

#define PACKET_TIMESTAMPED  (1 << 0)
#define PACKET_RECEIVED     (1 << 1)

#endif /* SEND_HISTORY_H */
