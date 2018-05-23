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
 *
 * generic thread set up and routine
 */

#include <poll.h> /* poll() */
#include <pthread.h>
#include <signal.h> /* kill() SIGINT */
#include <string.h> /* memset() */
#include <sys/eventfd.h> /* eventfd */
#include <sys/types.h>
#include <unistd.h> /* close() getpid() */

#include "thread_context.h"

/*
 * efd: evend fd (see eventfd.2 manual) here is used to
 * signal the thread to exit.
 *
 * NOTE: About returning -1 on error, poll() may have
 * succeed and revents = -1. However it's very unlikely
 * the revents mask we get equals -1
 */
static short
do_wait(int efd, int fd, short events)
{
	struct pollfd pfd[2];

	/* prepare */
	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = efd;
	pfd[0].events = POLLIN;
	pfd[1].fd = fd;
	pfd[1].events = events;

	/* poll */
	if (poll(pfd, 2, -1) == -1)
		return -1;

	/*
	 * if true, user have request us to quit
	 * Do not check _keep_running!
	 */
	if (pfd[0].revents & POLLIN)
		pthread_exit(0);

	return pfd[1].revents;
}

static int
generic_thread_run(struct thread_ctx *r)
{
	short revents;

	/* We're not checking for _keep_running anymore */
	while (1) {
		/*
		 * POLLERR is ignored because it's set by
		 * default. See poll.2 manual
		 */
		revents = do_wait(r->efd, r->fd, r->events);
		if (revents == -1)
			goto _go_exit_err;

		/* poll again if we haven't woken up with r->events */
		if (!(revents & r->events))
			continue;

		if (r->routine(r->data) == -1)
			goto _go_exit_err;
	}

	return 0;

_go_exit_err:
	/* shutdown other threads by sending SIGINT signal to the process */
	kill(getpid(), SIGINT);
	return -1;
}

static void*
generic_thread_routine(void *data)
{
	if (generic_thread_run((struct thread_ctx*) data) == 0)
		pthread_exit((void*) 0); /* ok */
	else
		pthread_exit((void*) 1); /* error */

	return NULL;
}

void
thread_context_cleanup(struct thread_ctx *c)
{
	close(c->efd);
}

int
thread_context_setup(struct thread_ctx *c, int (*routine)(void*),
                     void *data, int fd, short events)
{
	/* create event file descriptors used to signal threads to exit */
	c->efd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
	if (c->efd == -1)
		return -1;

	c->routine = routine;
	c->data =    data;
	c->fd =      fd;
	c->events =  events;

	return 0;
}

int
thread_terminate(struct thread_ctx *c)
{
	void *ret_ptr;

	eventfd_write(c->efd, 1);
	pthread_join(c->thread, &ret_ptr);

	if ((uintptr_t) ret_ptr != 0)
		return 1;

	return 0;
}

int
thread_start(struct thread_ctx *c)
{
	if (pthread_create(&c->thread, NULL, generic_thread_routine, c) != 0)
		return -1;

	return 0;
}
