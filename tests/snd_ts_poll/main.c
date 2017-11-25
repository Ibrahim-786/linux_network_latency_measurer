/*
 * 10/11/2017 Ricardo Biehl Pasquali
 * polling for send timestamp on socket's error queue
 */

#include <pthread.h> /* pthread_*() */

/* open_socket() do_send() do_poll() do_recv() */
#include "common.h"

/*
 * It's a thread. It does poll/recv, or send/poll/recv (when
 * SEND_IN_SAME_THREAD is defined)
 */
static void*
loop(void *data)
{
	int sfd = *((int*) data);

	for (;;) {
#ifdef SEND_IN_SAME_THREAD
		do_send(sfd);
#endif
		do_poll(sfd);
		do_recv(sfd);
	}

	return NULL;
}

int
main(int argc, char **argv)
{
	pthread_t loop_thread;
	int sfd;

	if ((sfd = open_socket()) == -1)
		return 1;

	/* create thread and wait for it to terminate */
	pthread_create(&loop_thread, NULL, loop, &sfd);
#ifndef SEND_IN_SAME_THREAD
	for (;;)
		do_send(sfd);
#endif
	pthread_join(loop_thread, NULL);

	return 0;
}
