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
 * 06/05/2018
 *
 * Receive packets and send them back
 *
 * compile with:
 * $ gcc -o mirror mirror.c
 */

#include <arpa/inet.h> /* htons */
#include <stdio.h> /* printf */
#include <stdint.h> /* uint64_t */
#include <stdlib.h> /* atoi */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> /* close */

int
main(int argc, char **argv)
{
	int fd;
	struct sockaddr_in saddr;
	uint16_t port;
	int tmp;
	uint64_t buf;
	socklen_t addrlen;

	if (argc < 2) {
		printf("usage: cmd <port>\n");
		return 1;
	}
	port = htons(atoi(argv[1]));

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1)
		return 1;

	saddr.sin_family = AF_INET;
	saddr.sin_port = port;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr*) &saddr, sizeof(saddr)) == -1)
		goto _go_close_fd;

	for (;;) {
		addrlen = sizeof(saddr);

		tmp = recvfrom(fd, &buf, sizeof(buf), 0,
		               (struct sockaddr*) &saddr, &addrlen);
		if (tmp == -1)
			break;

		/*
		 * It's returned source port in saddr.
		 * We want to send to the port specified
		 * in command line.
		 */
		saddr.sin_port = port;

		tmp = sendto(fd, &buf, sizeof(buf), 0,
		             (struct sockaddr*) &saddr, addrlen);
		if (tmp == -1)
			break;
	}

_go_close_fd:
	close(fd);

	return 0;
}
