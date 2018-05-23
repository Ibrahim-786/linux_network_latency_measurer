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
 * 30/04/2018
 *
 * receive packets from mirror and get their timestamps
 */

#include <pthread.h> /* pthread_mutex_*() */

/* for struct sockaddr_in */
#include <sys/socket.h>
#include <netinet/in.h>

#include <linux/errqueue.h> /* scm_timestamping */

#include "receiver.h"

#include "msgctx.h"
#include "result_buffer.h"
#include "send_history.h"
#include "time_common.h"

#define EFATAL  2

/* NOTE: attention to the endianness! */

static int
process_packet(struct receiver *r, struct msgctx *mctx)
{
	/* pointer to packet timestamp in control message */
	struct scm_timestamping *ts;

	struct timespec diff;

	uint64_t *packet_header;
	uint64_t id;
	struct sent_packet *send_info;
#ifndef WRITE_IN_SENDER
	struct sent_packet copy;
	struct result tmp_result;
#endif

	if (mctx->len < sizeof(uint64_t))
		goto _go_drop_packet;

	packet_header = mctx->data;

	id = *packet_header & 0x000000ffffffffff;
	/* error if packet ID is invalid */
	if (id >= r->send_history->packet_id_boundary)
		goto _go_drop_packet;

	/* get timestamp from message struct */
	ts = get_timestamp_from_msg(&mctx->msg);
	/* error if packet doesn't carry timestamp */
	if (ts == NULL)
		goto _go_drop_packet;

	/* NOTE: enter critical region */
	pthread_mutex_lock(&r->send_history->mtx);

	send_info =
	  &r->send_history->buffer[id % r->send_history->control.size];

	/*
	 * It's not necessary to check for a SENT flag
	 * because packets that are in the buffer were
	 * necessarily sent.
	 *
	 * If we don't check for PACKET_TIMESTAMPED, the
	 * first packet becomes an exception because its
	 * id is equal to the value used to initialize
	 * the buffer (zero), so it may be received
	 * without being sent.
	 */

	/*
	 * Error if packet id is invalid. Probably a
	 * timeout happened and the packet was already
	 * overwritten. TODO: log it
	 */
	if (send_info->id != id)
		goto _go_unlock_mutex_and_drop_packet;

	/*
	 * Error if packet was already received. Maybe the
	 * packet got duplicated! TODO: log it
	 */
	if (send_info->flags & PACKET_RECEIVED) {
		r->duplicate_packets++;
		goto _go_unlock_mutex_and_drop_packet;
	}

	/*
	 * Error if send timestamp did not arrive in storer.
	 * TODO: log it
	 */
	if (!(send_info->flags & PACKET_TIMESTAMPED))
		goto _go_unlock_mutex_and_drop_packet;

	/*
	 * Error if timeout was already reached.
	 * TODO: log it
	 */
	time_diff(&diff, &ts->ts[0], &send_info->ts);
	if (time_is_greater(&diff, &r->max_latency))
		goto _go_unlock_mutex_and_drop_packet;

#ifndef WRITE_IN_SENDER
	/* copy entry before exit the critical region */
	copy = *send_info;
#endif

#ifdef WRITE_IN_SENDER
	send_info->recv_ts = ts->ts[0];
#endif
	/* set received flag */
	send_info->flags |= PACKET_RECEIVED;

	/* NOTE: exit critical region */
	pthread_mutex_unlock(&r->send_history->mtx);

	r->valid_packets++;

	/*
	 * nsec_sum is used later to calculate the round
	 * trip latency average in nanoseconds (ns)
	 */
	r->nsec_sum += diff.tv_sec * 1000000000 + diff.tv_nsec;

#ifndef WRITE_IN_SENDER
	/*
	 * send the result to the writer
	 *
	 * Fatal error when a broken partial transfer
	 * happened. See result_buffer_insert_entry() in
	 * result_buffer.h
	 */
	tmp_result.id = copy.id;
	tmp_result.diff = diff;
	if (result_buffer_insert_entry(r->result_buffer, &tmp_result) == -1)
		return -EFATAL;
#endif

	return 0;

_go_unlock_mutex_and_drop_packet:
	pthread_mutex_unlock(&r->send_history->mtx);
_go_drop_packet:
	return -1;
}

int
receiver_do_its_job(struct receiver *r)
{
	/* process all packets we can read */
	while (msgctx_recv(r->sfd, &r->mctx, 0) == 0) {
		if (process_packet(r, &r->mctx) == -EFATAL)
			return -1;
	}

	return 0;
}

void
receiver_cleanup(struct receiver *r)
{
	msgctx_destroy(&r->mctx);
}

int
receiver_setup(struct receiver *r, unsigned int max_latency_ms)
{
	milliseconds_to_timespec(&r->max_latency, max_latency_ms);

	/* nsec_sum is used to calculate the average later */
	r->nsec_sum = 0;
	r->valid_packets = 0;
	r->duplicate_packets = 0;

	/* initialize buffer where we receive mirror reply */
	return msgctx_init(&r->mctx, 1500 /* MTU */, 1024,
	                   sizeof(struct sockaddr_in));
}
