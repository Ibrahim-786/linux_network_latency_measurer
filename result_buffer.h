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
 * (03,12)/05/2018
 */

#ifndef RESULT_BUFFER_H
#define RESULT_BUFFER_H

#include <stdint.h> /* int*_t */
#include <time.h> /* struct timespec */

/* TODO: add ID? */
struct result {
	uint64_t id;
	struct timespec diff;
	/* timestamp when packet was sent */
	//struct timespec sendts;
	/* timestamp when packet was received */
	//struct timespec recvts;
};

struct result_buffer {
	/* used for buffering results */
	struct result *buffer;
	unsigned int   size;
	unsigned int   boundary;
	unsigned int   index;

	/* log */
	unsigned int   misses;

	/* pipe ends file descriptors */
	int readfd;
	int writefd;
};

#ifdef WRITE_IN_SENDER
int
result_buffer_transfer(struct result_buffer *b);
#endif

int
result_buffer_insert_entry(struct result_buffer *b, struct result *result);

#endif /* RESULT_BUFFER_H */
