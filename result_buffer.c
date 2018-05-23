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
 * 20/05/2018
 *
 * do some buffering and send results to the writer
 */

#include <errno.h> /* EAGAIN */
#include <unistd.h> /* write() */

#include "result_buffer.h"

static int
transfer_to_writer(struct result_buffer *b, unsigned int size)
{
	int written;

	/* write to the writer's pipe */
	written = write(b->writefd, b->buffer, size);
	if (written != size) {
		if (written == -1) {
			/* something went wrong */
			if (errno == EAGAIN) {
				/* buffer is full */
				b->misses++;
				return 0;
			}
			return -1;
		} else if (written % sizeof(*b->buffer) != 0) {
			/*
			 * there was a partial write
			 * resulting in a broken result
			 * structure
			 */
			return -1;
		}
	}

	return 0;
	
}

#ifdef WRITE_IN_SENDER
int
result_buffer_transfer(struct result_buffer *b)
{
	int size = b->index * sizeof(*b->buffer);

	if (size == 0)
		return 0;

	return transfer_to_writer(b, size);
}
#endif

/*
 * At the moment only one thread calls
 * result_buffer_insert_entry(), so a mutex is not needed.
 */

int
result_buffer_insert_entry(struct result_buffer *b, struct result *result)
{
	/*
	 * if local buffer is not full, just insert one
	 * more entry and return
	 */
	b->buffer[b->index] = *result;
	b->index++;
	if (b->index != b->boundary)
		return 0;

	/*
	 * the buffer is full, let's transfer it to the
	 * writer
	 */

	b->index = 0;

	return transfer_to_writer(b, b->size);
}
