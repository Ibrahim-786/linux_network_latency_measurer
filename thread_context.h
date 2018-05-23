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
 * 16/05/2018
 */

#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include <pthread.h>

struct thread_ctx {
	int (*routine)(void*);
	void *data;

	pthread_t thread;

	int   fd;
	short events;

	int   efd;
};

void
thread_context_cleanup(struct thread_ctx *c);

int
thread_context_setup(struct thread_ctx *c, int (*routine)(void*),
                     void *data, int fd, short events);

int
thread_terminate(struct thread_ctx *c);

int
thread_start(struct thread_ctx *c);

#endif /* THREAD_CONTEXT_H */
