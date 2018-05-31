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
 * 04/2018
 *
 * read the results from a pipe and write them to a file
 */

#include <stdio.h> /* FILE* fopen() fclose() fflush() */
#include <unistd.h> /* read() */

#include "writer.h"

#include "result_buffer.h" /* struct result_buffer */

#define COPY_BUFFER_SIZE  128

static void
do_output(struct writer *w, struct result *r)
{
	uint64_t tmp[2];

	/*
	 * Here is a place of the code you may want to
	 * edit. This is the where measurements are
	 * output. You can even use a printf() here :-)
	 */

	switch (w->output_type) {
	case WRITER_OUTPUT_FRIENDLY:
		if (!r->diff.tv_sec && !r->diff.tv_nsec) {
			fprintf(w->file, "%ld Error!\n", r->id);
			return;
		}
		fprintf(w->file, "%ld %ld.%06ld ms\n",
		        r->id,
		        r->diff.tv_sec * 1000 + r->diff.tv_nsec / 1000000,
		        r->diff.tv_nsec % 1000000);
		break;
	case WRITER_OUTPUT_CSV:
		if (!r->diff.tv_sec && !r->diff.tv_nsec) {
			fprintf(w->file, "%ld,error\n", r->id);
			return;
		}
		/* microseconds */
		fprintf(w->file, "%ld,%ld\n", r->id,
			r->diff.tv_sec * 1000000 + r->diff.tv_nsec / 1000);
		break;
	case WRITER_OUTPUT_BINARY:
		/* microseconds */
		tmp[0] = r->id;
		tmp[1] = r->diff.tv_sec * 1000000 + r->diff.tv_nsec / 1000;
		fwrite(tmp, sizeof(tmp), 1, w->file);
		break;
	default:
		break;
	}
}

static int
file_write(struct writer *w, struct result *buffer, int len)
{
	int i;

	//fprintf(w->file, "[buffer len = %d]\n", len);
	for (i = 0; i < len; i++)
		do_output(w, &buffer[i]);

	return 0;
}

/*
 * The writer reads at most COPY_BUFFER_SIZE entries from
 * the pipe and writes them to the file. If the other side
 * writes just one entry to the pipe, the writer will end
 * up writing just one entry to the file. In order to
 * avoid this, a buffering is done when the other side
 * writes using result_buffer_insert_entry() from
 * result_buffer.h
 */
int
writer_do_its_job(struct writer *w)
{
	struct result copy[COPY_BUFFER_SIZE];
	int bytes_copied;

	bytes_copied = read(w->result_buffer->readfd, copy, sizeof(copy));
	if (bytes_copied == -1) {
		/* TODO: what to do? */
		return 0;
	}
	
	file_write(w, copy, bytes_copied / sizeof(*copy));

	return 0;
}

void
writer_cleanup(struct writer *w)
{
	/* write buffered data and close file */
	fflush(w->file);
	if (w->file != stdout)
		fclose(w->file);
}

int
writer_setup(struct writer *w, char *writer_file)
{
	if (writer_file == NULL) {
		w->file = stdout;
	} else {
		/*
		 * 'w': Truncate file to zero length or
		 * create text file for writing. The
		 * stream is positioned at the beginning
		 * of the file.
		 * 'x': Fails if writer_file already
		 * exists.
		 */
		w->file = fopen(writer_file, "wx");
		if (w->file == NULL)
			return -1;

		/*
		 * TODO: Perhaps improve buffering using
		 * setbuf(). See setbuf.3 manual
		 */
	}

	return 0;
}
