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

#ifndef WRITER_H
#define WRITER_H

#include <stdio.h> /* FILE* */

#include "result_buffer.h" /* struct result_buffer */

#define WRITER_OUTPUT_FRIENDLY  0
#define WRITER_OUTPUT_CSV       1
#define WRITER_OUTPUT_BINARY    2

struct writer {
	/* from main */
	struct result_buffer *result_buffer;
	int output_type;

	/* the file where writer will write */
	FILE *file;
};

int
writer_do_its_job(struct writer *w);

void
writer_cleanup(struct writer *w);

int
writer_setup(struct writer *w, char *writer_file);

#endif /* WRITER_H */
