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

#ifndef MSGCTX_H
#define MSGCTX_H

/* TODO: is it necessary to include the two headers below? */
#include <sys/types.h>
#include <sys/socket.h>

struct msgctx {
	/* memory used in recvmsg() */
	void *memory;

	struct msghdr msg;
	/* the packet payload (IO vector->base will point to packet_header) */
	struct iovec iov;

	/*
	 * pointer to the buffer where the data is
	 * i.e. an alias to iov.iov_base
	 */
	void *data;

	/*
	 * keep name_len and control_len because
	 * they can be modified by the kernel
	 */
	size_t name_len;
	size_t control_len;

	/* packet data length */
	size_t len;
};

int
msgctx_recv(int sfd, struct msgctx *mctx, int flags);

void
msgctx_destroy(struct msgctx *mctx);

int
msgctx_init(struct msgctx *mctx, size_t buffer_len, size_t control_len,
            size_t addr_len);

#endif /* MSGCTX_H */
