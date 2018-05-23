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
 * 14/05/2018
 */

#include <poll.h> /* POLL* */
#include <signal.h> /* sigfillset() sigwait() */
#include <stdio.h> /* printf() */

#include "measurer_elements.h"

#include "receiver.h" /* receiver_thread_routine() */
#include "storer.h" /* storer_thread_routine() */
#include "sender.h" /* sender_thread_routine() */
#include "thread_context.h" /* struct thread_ctx */

static void
wait_for_signal(void)
{
	sigset_t mask;
	int sig = 0;

	while (1) {
		/* mask all signals and wait */
		sigfillset(&mask);
		sigwait(&mask, &sig);

		switch (sig) {
		case SIGINT:
		case SIGQUIT:
			return;
		}
	}
}

/*
 * the sender waits for timer expiration
 *
 * ----------
 *
 * the storer waits for timestamps of packets we've sent
 *
 * We poll for messages in socket's error queue, where our
 * send-timestamp will be returned in a control message
 * (aka cmsg). Packet payload is also returned, allowing us
 * to know the id to correlate with timestamp.
 *
 * ----------
 *
 * the receiver waits for packets from mirror
 *
 * NOTE: it can read packets before storer store their
 * timestamps. A delay introduced before routine solves
 * this problem.  usleep(1000);
 */

#define set_and_goto(var, val, label) \
	do { \
		var = val; \
		goto label; \
	} while (0)

enum {
	RECEIVER,
	STORER,
	SENDER,
	LAST,
};

int
multithread_run(struct measurer_elements *e)
{
	int ret = 0;
	struct thread_ctx threads[LAST];
	int i;

	if (thread_context_setup(&threads[RECEIVER],
	    (void*) receiver_do_its_job, e->receiver,
	    e->receiver->sfd, POLLIN) == -1)
		return 1;

	if (thread_context_setup(&threads[STORER],
	    (void*) storer_do_its_job, e->storer,
	    e->storer->sfd, POLLPRI) == -1)
		set_and_goto(ret, 1, _go_cleanup_receiver);

	if (thread_context_setup(&threads[SENDER],
	    (void*) sender_do_its_job, e->sender,
	    e->sender->tfd, POLLIN) == -1)
		set_and_goto(ret, 1, _go_cleanup_storer);

	/* start threads */
	for (i = 0; i < LAST; i++) {
		if (thread_start(&threads[i]) == -1)
			set_and_goto(ret, 1, _go_terminate_threads);
	}

	/* start sender timer fd */
	sender_timer_start(e->sender);

	printf("All threads started\n");

	/* wait for signal, then proceed exiting */
	wait_for_signal();

	printf("Terminating threads\n");

_go_terminate_threads:
	/* wake up and wait for all threads to terminate */
	while (i--) {
		if (thread_terminate(&threads[i]))
			ret = 1;
	}

//_go_cleanup_sender:
	thread_context_cleanup(&threads[SENDER]);
_go_cleanup_storer:
	thread_context_cleanup(&threads[STORER]);
_go_cleanup_receiver:
	thread_context_cleanup(&threads[RECEIVER]);

	return ret;
}
