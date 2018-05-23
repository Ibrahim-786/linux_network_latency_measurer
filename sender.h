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

#ifndef SENDER_H
#define SENDER_H

#include <stdint.h> /* uint64_t */
#include <time.h> /* struct timespec */
#include <netinet/in.h> /* struct sockaddr_in */

#include "send_history.h"
#ifdef WRITE_IN_SENDER
#include "result_buffer.h"
#endif

struct sender {
	/* from main */
#ifdef WRITE_IN_SENDER
	struct result_buffer *result_buffer;
#endif
	struct send_history *send_history;
	int sfd;
#ifdef SEND_COUNT
	unsigned int send_count;
	unsigned int max_latency;
#endif
	struct sockaddr_in addr;

	int tfd; /* timer fd */

	/* sleep interval */
	struct timespec sleep_interval;
	/* number of packets to send per run */
	unsigned int packet_count;

	/* runtime information */
	uint64_t current_id;

#ifdef SEND_COUNT
	int exit_sender;
#endif

	/* log */
	uint64_t total_packets_sent;
};

#ifdef WRITE_IN_SENDER
int
sender_flush_send_history(struct sender *s);
#endif

int
sender_do_its_job(struct sender *s);

void
sender_timer_start(struct sender *s);

void
sender_cleanup(struct sender *s);

int
sender_setup(struct sender *s, unsigned int sleep_ms,
             unsigned int packet_count);

#endif /* SENDER_H */
