/*
 * 19/11/2017 Ricardo Biehl Pasquali
 * polling for send timestamp on socket's error queue
 *
 * do_poll(), do_recv, do_send(), open_socket()
 */

#include <arpa/inet.h> /* hton*() */
#include <poll.h> /* poll() */
#include <string.h> /* memset() */
#include <sys/types.h> /* socket() recv() */
#include <sys/socket.h> /* socket() recv() */
#include <unistd.h> /* close() */

#include <linux/net_tstamp.h> /* timestamp stuff */

#include "common.h"

int
do_poll(int sfd, short request_mask)
{
	struct pollfd pfd;

	/* prepare */
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = sfd;
	pfd.events = request_mask;

	/* poll */
	return poll(&pfd, 1, -1);
}

void
do_recv(int sfd)
{
	char tmp;

	while (recv(sfd, &tmp, sizeof(tmp), 0)            == sizeof(tmp));

	while (recv(sfd, &tmp, sizeof(tmp), MSG_ERRQUEUE) == sizeof(tmp));
}

void
do_send(int sfd)
{
	struct sockaddr_in saddr;
	char tmp = 'a';

	/* prepare */
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(PORT);
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	sendto(sfd, &tmp, sizeof(tmp), 0, (struct sockaddr*) &saddr,
	       sizeof(saddr));
}

/*
 * SO_SELECT_ERR_QUEUE: wake the socket with `POLLPRI|POLLERR` if it is in the
 * error list, which would enable software to wait on error queue packets
 * without waking up for regular data on the socket
 */
static int
set_pollpri_on_errqueue(int sfd)
{
	int on = 1;
	int tmp;

	tmp = setsockopt(sfd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &on, sizeof(on));
	if (tmp == -1)
		return -1;

	return 0;
}

/* SO_TIMESTAMPING: timestamp packets */
static int
set_ts_opt(int sfd)
{
	unsigned int opt;

	/* set timestamp option */
	opt = SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_TX_SCHED |
	      SOF_TIMESTAMPING_OPT_CMSG;
	if (setsockopt(sfd, SOL_SOCKET, SO_TIMESTAMPING, (char*) &opt,
	               sizeof(opt)) == -1)
		return -1;

	return 0;
}

int
open_socket(int flags)
{
	int sfd;
	int tmp;

	/* open socket in non-blocking mode */
	sfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	if (sfd == -1)
		return -1;

	if (flags & BIND_SOCKET) {
		struct sockaddr_in saddr;

		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(PORT);
		saddr.sin_addr.s_addr = htonl(INADDR_ANY);

		tmp = bind(sfd, (const struct sockaddr*) &saddr,
		           sizeof(saddr));
		if (tmp == -1)
			goto _go_close_socket;
	}

	if (flags & POLLPRI_WAKEUP_ON_ERROR_QUEUE) {
		tmp = set_pollpri_on_errqueue(sfd);
		if (tmp == -1)
			goto _go_close_socket;
	}

	if (flags & ENABLE_TX_TIMESTAMP) {
		tmp = set_ts_opt(sfd);
		if (tmp == -1)
			goto _go_close_socket;
	}

	return sfd;

_go_close_socket:
	close(sfd);
	return -1;
}
