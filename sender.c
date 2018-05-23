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
 * 09/2017
 *
 * send packets
 *
 * - If WRITE_IN_SENDER is defined, results are sent to
 *   the writer here.
 * - If SEND_COUNT is defined, we allow the user to
 *   specify the number of packets to send before exit.
 *
 * 17:21 23/10/2017: revised
 * 16:08 03/05/2018: revised
 */

#include <netinet/in.h> /* struct sockaddr_in */
#include <pthread.h> /* pthread_mutex_*() */
#include <stdint.h> /* int*_t */
#include <stdio.h> /* printf */
#include <sys/socket.h> /* sendto() */
#include <sys/timerfd.h> /* timerfd_*() */
#include <sys/types.h> /* send() */
#include <unistd.h> /* close() read() */

#include "sender.h"

#include "send_history.h"
#ifdef WRITE_IN_SENDER
#include "result_buffer.h"
#include "time_common.h"
#endif

#ifdef WRITE_IN_SENDER
int
sender_flush_send_history(struct sender *s)
{
	struct sent_packet *entry;
	struct timespec     diff;
	struct result tmp_result;
	int i;

	i = s->send_history->control.current;

	do {
		entry = &s->send_history->buffer[i];

		if (entry->flags & PACKET_TIMESTAMPED
		    && entry->flags & PACKET_RECEIVED) {
			time_diff(&diff, &entry->recv_ts, &entry->ts);
			tmp_result.id = entry->id;
			tmp_result.diff = diff;
			if (result_buffer_insert_entry(s->result_buffer,
			    &tmp_result) == -1)
				return -1;
		}

		if (++i == s->send_history->control.size)
			i = 0;

		/* TODO: should we call writer_do_its_job() here? */

	} while (i != s->send_history->control.current);

	/* flush result buffer */
	result_buffer_transfer(s->result_buffer);

	return 0;
}
#endif

#ifdef SEND_COUNT
static void
set_timer(struct sender *s, unsigned int ms)
{
	struct itimerspec interval;

	interval.it_value.tv_sec = (ms / 1000);
	interval.it_value.tv_nsec = (ms % 1000) * 1000000;
	interval.it_interval.tv_sec = 0;
	interval.it_interval.tv_nsec = 0;

	timerfd_settime(s->tfd, 0, &interval, NULL);
}
#endif

int
sender_do_its_job(struct sender *s)
{
	/* temporary */
	int i;
	int tmp;
	uint64_t timer_overruns;
	uint64_t packet_header;
	struct sent_packet *entry;

#ifdef WRITE_IN_SENDER
	/* used for writing results */
	struct sent_packet  copy;
	struct timespec     diff;
	struct result       tmp_result;
#endif

	/* get timer overrun counter */
	tmp = read(s->tfd, &timer_overruns, sizeof(timer_overruns));
	if (tmp != sizeof(timer_overruns))
		return 0;
	if (timer_overruns == 0) {
		return 0;
	} else if (timer_overruns > 1) {
		printf("oops, multiple timer overruns (overruns: %ld)",
		       timer_overruns);
		return -1; /* exit ERROR */
	}

#ifdef SEND_COUNT
	/* TODO: make it return an OK status */
	if (s->exit_sender)
		return -1;
#endif

	/* send packets */
	for (i = 0; i < s->packet_count; i++) {
		packet_header = s->current_id & PACKET_ID_MASK;
		/* NOTE: set flags (0xffffff0000000000) here */

		/*
		 * Put the id in ring buffer
		 * before send. If we put the id
		 * after send, the storer may wake
		 * up before we put it and then
		 * see inconsistent data.
		 *
		 * Timestamp messages may arrive out
		 * of order in storer
		 */

		/* NOTE: enter critical region */
		pthread_mutex_lock(&s->send_history->mtx);

		entry =
		&s->send_history->buffer[s->send_history->control.current];

#ifdef WRITE_IN_SENDER
		copy = *entry;
#endif

		entry->id = s->current_id;
		entry->flags = 0;

		single_ring_buffer_update(&s->send_history->control);

		/* NOTE: exit critical region */
		pthread_mutex_unlock(&s->send_history->mtx);

		/* if send fails we quit the program */
		tmp = sendto(s->sfd, &packet_header,
			     sizeof(packet_header), 0,
			     (struct sockaddr*) &s->addr, sizeof(s->addr));
		if (tmp != sizeof(packet_header))
			return -1;

		/* increment a counter of sent packets */
		s->total_packets_sent++;

		if (++s->current_id == s->send_history->packet_id_boundary)
			s->current_id = 0;

#ifdef WRITE_IN_SENDER
		/* TODO: it must print packets that have missed */
		if (copy.flags & PACKET_TIMESTAMPED
		    && copy.flags & PACKET_RECEIVED) {
			time_diff(&diff, &copy.recv_ts, &copy.ts);
			tmp_result.id = copy.id;
			tmp_result.diff = diff;
			if (result_buffer_insert_entry(s->result_buffer,
			    &tmp_result) == -1)
				return -1;
		}
#endif

#ifdef SEND_COUNT
		if (s->send_count != -1
		    && s->total_packets_sent == s->send_count) {
			set_timer(s, s->max_latency);
			s->exit_sender = 1;
			return 0;
		}
#endif

	}

	return 0;
}

void
sender_timer_start(struct sender *s)
{
	struct itimerspec interval;

	interval.it_value = s->sleep_interval;
	interval.it_interval = s->sleep_interval;

	timerfd_settime(s->tfd, 0, &interval, NULL);
}

void
sender_cleanup(struct sender *s)
{
	close(s->tfd);
}

int
sender_setup(struct sender *s, unsigned int sleep_ms,
             unsigned int packet_count)
{
	/* timerfd allows waiting for expiration on a file descriptor */
	s->tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (s->tfd == -1)
		return -1;

	/* convert milliseconds to struct timespec */
	s->sleep_interval.tv_sec = sleep_ms / 1000;
	s->sleep_interval.tv_nsec = (sleep_ms % 1000) * 1000000;

	/* number of packets to send on every timer expiration */
	s->packet_count = packet_count;

	s->current_id = 0;

#ifdef SEND_COUNT
	/*
	 * after sending the N packets requested by user
	 * we set it to 1
	 */
	s->exit_sender = 0;
#endif

	s->total_packets_sent = 0;

	return 0;
}
