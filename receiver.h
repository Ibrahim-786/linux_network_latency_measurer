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

#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdint.h> /* uint64_t */
#include <time.h>

#include "msgctx.h"
#include "result_buffer.h"
#include "send_history.h"

struct receiver {
	/* from main */
	struct result_buffer *result_buffer;
	struct send_history *send_history;
	int sfd;

	struct timespec max_latency;
	struct msgctx mctx;

	/* log */
	uint64_t nsec_sum;
	uint64_t valid_packets;
	uint64_t duplicate_packets;
};

int
receiver_do_its_job(struct receiver *r);

void
receiver_cleanup(struct receiver *r);

int
receiver_setup(struct receiver *r, unsigned int max_latency_ms);

#endif /* RECEIVER_H */
