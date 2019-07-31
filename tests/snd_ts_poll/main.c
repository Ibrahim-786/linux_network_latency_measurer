/*
 * 10/11/2017 Ricardo Biehl Pasquali
 * polling for send timestamp on socket's error queue
 */

#include <getopt.h>  /* getopt_long() */
#include <poll.h>    /* POLL* */
#include <pthread.h> /* pthread_*() */
#include <stdio.h>   /* printf() */
#include <string.h>  /* strcmp() */
#include <unistd.h>  /* sleep(), getopt_long() */

/* open_socket() do_send() do_poll() do_recv() */
#include "common.h"

static int send_in_same_thread = 0;
static int use_select = 0;
static short request_mask = 0;

/*
 * It's a thread. It does poll/recv, or send/poll/recv (when
 * SEND_IN_SAME_THREAD is defined)
 */
static void*
loop(void *data)
{
	int sfd = *((int*) data);

	for (;;) {
		if (send_in_same_thread) {
			sleep(1);
			do_send(sfd);
		}

		if (use_select)
			do_select(sfd, request_mask);
		else
			do_poll(sfd, request_mask);
		do_recv(sfd);
	}

	return NULL;
}

int
main(int argc, char **argv)
{
	pthread_t loop_thread;
	int sfd;
	int flags = 0;

	while (1) {
		int c;
		struct option long_options[] = {
			{ "help",          no_argument, 0, 'h' },
			{ "pollpri",       no_argument, 0, 'p' },
			{ "pollin",        no_argument, 0, 'i' },
			{ "pollerr",       no_argument, 0, 'e' },
			{ "single-thread", no_argument, 0, 's' },
			{ "use-select",    no_argument, 0, 'S' },
			{ "mask-pollpri",  no_argument, 0, 'm' },
			{ "bind-socket",   no_argument, 0, 'b' },
			{ "tx-timestamp",  no_argument, 0, 't' },
			{ 0,               0,           0, 0   },
		};

		c = getopt_long(argc, argv, "", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			c = 0;
			while (long_options[c].name)
				printf("--%s\n", long_options[c++].name);
			return 0;
		case 'p': request_mask |= POLLPRI; break;
		case 'i': request_mask |= POLLIN;  break;
		case 'e': request_mask |= POLLERR; break;
		case 's': send_in_same_thread = 1; break;
		case 'S': use_select = 1;          break;
		case 'm': flags |= POLLPRI_WAKEUP_ON_ERROR_QUEUE; break;
		case 'b': flags |= BIND_SOCKET;                   break;
		case 't': flags |= ENABLE_TX_TIMESTAMP;           break;
		}
	}

	if ((sfd = open_socket(flags)) == -1)
		return 1;

	/* create thread and wait for it to terminate */
	pthread_create(&loop_thread, NULL, loop, &sfd);
	if (!send_in_same_thread) {
		for (;;) {
			sleep(1);
			do_send(sfd);
		}
	}

	pthread_join(loop_thread, NULL);

	return 0;
}
