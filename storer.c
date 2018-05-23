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
 * 10/2017
 *
 * store tx (send) timestamps
 *
 * 18:02 23/10/2017: revised
 */

#include <stdint.h> /* int*_t */
#include <time.h> /* struct timespec */

#include <linux/errqueue.h> /* struct scm_timestamping */

#include <linux/if_ether.h> /* for ethernet header */
#include <linux/ip.h> /* for ipv4 header */
#include <linux/udp.h> /* for upd header */

#include "storer.h"

#include "msgctx.h" /* msgctx_*() */
#include "send_history.h" /* struct send_history */
#include "time_common.h" /* get_timestamp_from_msg() */

/* ethernet (packed) + ipv4 (aligned) + udp (aligned) */
/* TODO: can ipv4 header be bigger than its structure? */
#define HEADER_SIZE (sizeof(struct ethhdr) + \
                     sizeof(struct iphdr) + \
                     sizeof(struct udphdr))

static int
process_packet(struct storer *s, struct msgctx *mctx)
{
	/* pointer to packet timestamp in control message */
	struct scm_timestamping *ts;
	uint64_t id;
	//uint64_t flags;
	struct sent_packet *tmp;

	/*
	 * error if packet lenght is less than expected
	 *
	 * HEADER_SIZE refers to eth, ip and udp headers.
	 */
	if (mctx->len < HEADER_SIZE + sizeof(*s->packet_header))
		goto _go_drop_packet;

	id = *s->packet_header & 0x000000ffffffffff;
	//flags = *s->packet_header & 0xffffff0000000000;

	/* get timestamp from message struct */
	ts = get_timestamp_from_msg(&mctx->msg);
	/* error if packet doesn't carry timestamp */
	if (ts == NULL)
		goto _go_drop_packet;

	/* NOTE: enter critical region */
	pthread_mutex_lock(&s->send_history->mtx);

	tmp = &s->send_history->buffer[id % s->send_history->control.size];

	/*
	 * error if packet id is invalid
	 *
	 * Probably the sender has already overwritten
	 * the entry before we can even read it. In this
	 * case a bigger buffer size (max_latency) solves
	 * the problem. Otherwise, it's very unexpected.
	 */
	if (tmp->id != id)
		goto _go_unlock_mutex_and_drop_packet;

	tmp->ts = ts->ts[0];

	/* set a flag that entry has been timestamped */
	tmp->flags |= PACKET_TIMESTAMPED;

	/* NOTE: exit critical region */
	pthread_mutex_unlock(&s->send_history->mtx);

	s->total_packets_stored++;

	return 0;

_go_unlock_mutex_and_drop_packet:
	pthread_mutex_unlock(&s->send_history->mtx);
_go_drop_packet:
	return -1;
}

int
storer_do_its_job(struct storer *s)
{
	/*
	 * process all packets we can read
	 *
	 * NOTE: Perhaps we don't need to use a
	 * different socket for receiver thread as
	 * it won't read MSG_ERRQUEUE, so our
	 * recvmsg() probably won't fail. However,
	 * performance seems to be affected. See
	 * "Implementation FAQ" in README.
	 */
	while (msgctx_recv(s->sfd, &s->mctx, MSG_ERRQUEUE) == 0)
		process_packet(s, &s->mctx);

	return 0;
}

void
storer_cleanup(struct storer *s)
{
	msgctx_destroy(&s->mctx);
}

int
storer_setup(struct storer *s)
{
	int t;

	/*
	 * msgctx_init(..., data_size, control_size, addr_size);
	 * We set addr_size to zero because we don't want
	 * address field. In `control_size`, we might use
	 * `CMSG_SPACE( sizeof(struct scm_timestamping) )`
	 * but it may not be sufficient if more than one
	 * cmsg arrives.
	 */
	t = msgctx_init(&s->mctx, HEADER_SIZE + sizeof(uint64_t), 1024, 0);
	if (t == -1)
		return -1;

	s->packet_data = s->mctx.msg.msg_iov[0].iov_base;
	s->packet_header = (void*) s->packet_data + HEADER_SIZE;

	s->total_packets_stored = 0;

	return 0;
}
