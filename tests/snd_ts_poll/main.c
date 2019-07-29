/*
 * 10/11/2017 Ricardo Biehl Pasquali
 * polling for send timestamp on socket's error queue
 */

#include <poll.h>    /* POLL* */
#include <pthread.h> /* pthread_*() */
#include <string.h>  /* strcmp() */
#include <unistd.h>  /* sleep() */

/* open_socket() do_send() do_poll() do_recv() */
#include "common.h"

static int send_in_same_thread = 0;
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

	while (--argc) {
		if (strcmp("POLLPRI", argv[argc]) == 0)
			request_mask |= POLLPRI;
		else if (strcmp("POLLIN", argv[argc]) == 0)
			request_mask |= POLLIN;
		else if (strcmp("POLLERR", argv[argc]) == 0)
			request_mask |= POLLERR;
		else if (strcmp("SEND_IN_SAME_THREAD", argv[argc]) == 0)
			send_in_same_thread = 1;
		else if (strcmp("POLLPRI_WAKEUP", argv[argc]) == 0)
			flags |= POLLPRI_WAKEUP_ON_ERROR_QUEUE;
		else if (strcmp("BIND_SOCKET", argv[argc]) == 0)
			flags |= BIND_SOCKET;
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
