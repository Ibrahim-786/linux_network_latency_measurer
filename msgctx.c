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
 * 01/12/2017
 *
 * See include/linux/socket.h (from in Linux kernel tree)
 *
 * Also, see:
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_socket.h.html
 */

#include <string.h> /* memset() */
#include <stdlib.h> /* calloc() */
#include <sys/socket.h> /* recvmsg() */
#include <sys/types.h> /* recvmsg() */

#include "msgctx.h"

int
msgctx_recv(int sfd, struct msgctx *mctx, int flags)
{
	/* message struct */
	struct msghdr *msg = &mctx->msg;
	int t;

	/*
	 * NOTE
	 *
	 * Only msg_namelen and msg_controllen need to be
	 * reset. See ___sys_recvmsg() on net/socket.c
	 * from Linux kernel tree.
	 *
	 * if msg_namelen is greater than kernel expects,
	 * move_addr_to_user() truncates it
	 *
	 * msg_controllen and msg_flags can be also changed
	 * by the kernel
	 */

	/* reset lengths */
	msg->msg_namelen = mctx->name_len;
	msg->msg_controllen = mctx->control_len;
	/* reset control buffer TODO: is it necessary? */
	memset(msg->msg_control, 0, mctx->control_len);

	t = recvmsg(sfd, msg, flags);
	if (t < 0)
		return -1;

	mctx->len = t;

	return 0;
}

void
msgctx_destroy(struct msgctx *mctx)
{
	free(mctx->memory);
}

/*
 * msghdr structure has information about the packet
 *
 * iovec structure has pointers to buffers where
 * data will be transfered. msghdr has a pointer
 * to an iovec
 */
int
msgctx_init(struct msgctx *mctx, size_t buffer_len, size_t control_len,
            size_t addr_len)
{
	struct msghdr *msg       = &mctx->msg;
	struct iovec  *io_vector = &mctx->iov;

	void *tmp;

	/* TODO: use mlockall(MCL_CURRENT | MCL_FUTURE) to lock memory */

	mctx->memory = calloc(1, addr_len + control_len + buffer_len);

	tmp = mctx->memory;

	/* set up space where peer address is received */
	mctx->name_len = addr_len;
	msg->msg_name = addr_len ? tmp : NULL;
	tmp += addr_len;

	/*
	 * set up control message space
	 * See `man 3 cmsg` for more details about control messages
	 */
	mctx->control_len = control_len;
	msg->msg_control = control_len ? tmp : NULL;
	tmp += control_len;

	/* set up the only IO vector being used */
	io_vector[0].iov_len = buffer_len;
	io_vector[0].iov_base = tmp;
	mctx->data = tmp;
	tmp += buffer_len;

	/*
	 * say that only one IO vector is being used and
	 * store its pointer
	 */
	msg->msg_iovlen = 1;
	msg->msg_iov = io_vector;

	return 0;
}
