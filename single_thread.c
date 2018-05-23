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
 * 11/05/2018
 *
 * polling run
 */

#include <poll.h>
#include <signal.h> /* sigfillset */
#include <sys/signalfd.h>
#include <unistd.h> /* close() */

#include "single_thread.h"
#include "measurer_elements.h"

#include "sender.h" /* sender_do_its_job() */
#include "storer.h" /* storer_do_its_job() */
#include "receiver.h" /* receiver_do_its_job() */

enum {
	SIGNAL_FD,
	TIMER_FD,
	SEND_FD,
	RECV_FD,
};

static void
setup_poll_fd(struct pollfd *pfd, int fd, short events)
{
	pfd->fd = fd;
	pfd->events = events;
}

static inline void
clean_revents(struct pollfd *pfd, int n)
{
	while (n--)
		pfd[n].revents = 0;
}

/*
 * Check order: signal, send timer, send timestamp,
 * receive.
 */
static int
run(struct measurer_elements *e, int signal_fd)
{
	struct pollfd pfd[4];
	int keep_running = 1;

	/* temporary */
	int tmp;

	/* setup poll file descriptors */

	setup_poll_fd(&pfd[SIGNAL_FD], signal_fd,             POLLIN);
	setup_poll_fd(&pfd[TIMER_FD],  e->sender->tfd,        POLLIN);
	/* maybe POLLIN when SO_SELECT_ERRQUEUE is not available */
	setup_poll_fd(&pfd[SEND_FD],   e->sender->sfd,   POLLPRI);
	setup_poll_fd(&pfd[RECV_FD],   e->receiver->sfd, POLLIN);

	sender_timer_start(e->sender);

	while (keep_running) {
		/* wait (poll) for an event */
		clean_revents(pfd, 4);
		tmp = poll(pfd, 4, -1);
		if (tmp == -1)
			goto _go_exit_err;

		if (pfd[SIGNAL_FD].revents & POLLIN) {
			/* check the signal and exit */
			/* Exiting here. TODO: Maybe it's temporary. */
			keep_running = 0;
			continue;
		}

		if (pfd[TIMER_FD].revents & POLLIN) {
			/*
			 * send the packet and store its id in ring buffer
			 *
			 * Idea: perhaps we can send the result to the
			 * writer here, when we overwrite IDs.
			 *
			 * probably the writer is a separate thread
			 * and we'll communicate to it using
			 * m->pipe[1] variable.
			 */

			if (sender_do_its_job(e->sender) == -1)
				goto _go_exit_err;
		}

		/* maybe POLLIN when SO_SELECT_ERRQUEUE is not available */
		if (pfd[SEND_FD].revents & POLLPRI) {
			if (storer_do_its_job(e->storer) == -1)
				goto _go_exit_err;
		}

		if (pfd[RECV_FD].revents & POLLIN) {
			/* receive packet and store timestamp in ring buffer */
			if (receiver_do_its_job(e->receiver) == -1)
				goto _go_exit_err;
		}
	}

	return 0;

_go_exit_err:
	return -1;
}

#define set_and_goto(var, val, label) \
	do { \
		var = val; \
		goto label; \
	} while (0)

int
singlethread_run(struct measurer_elements *e)
{
	int ret = 0;
	sigset_t mask;
	int signal_fd;

	/* signal fd */
	sigfillset(&mask);
	signal_fd = signalfd(-1, &mask, SFD_NONBLOCK);
	if (signal_fd == -1)
		return 1;

	if (run(e, signal_fd) == -1)
		set_and_goto(ret, 1, _go_close_signalfd);

_go_close_signalfd:
	close(signal_fd);
	return ret;
}
