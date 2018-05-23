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
 * 10/05/2018
 *
 * helpers for time calculations and timestamps
 */

#include <sys/types.h> /* struct msghdr */
#include <time.h> /* struct timespec */
#include <linux/errqueue.h> /* scm_timestamping */

#define for_each_cmsg(msg, cmsg) \
	for (cmsg = CMSG_FIRSTHDR(msg); \
	     cmsg && cmsg->cmsg_len; \
	     cmsg = CMSG_NXTHDR(msg, cmsg))

/*
 * NOTE: Parentheses around && are not needed, however we
 * use them to avoid confusion and the warning produced by
 * gcc.
 */
#define time_is_greater(a, b) \
	( (a)->tv_sec > (b)->tv_sec || \
	  ( (a)->tv_sec == (b)->tv_sec && (a)->tv_nsec > (b)->tv_nsec ) )

static inline void
time_diff(struct timespec *diff,
          struct timespec *stop,
          struct timespec *start)
{
	if (stop->tv_nsec < start->tv_nsec) {
		/* here we assume (stop->tv_sec - start->tv_sec) is not zero */
		diff->tv_sec = stop->tv_sec - start->tv_sec - 1;
		diff->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		diff->tv_sec = stop->tv_sec - start->tv_sec;
		diff->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

static inline void
milliseconds_to_timespec(struct timespec *out, unsigned int ms)
{
	out->tv_nsec = ms * 1000000;
	out->tv_sec = out->tv_nsec / 1000000000;
	out->tv_nsec -= out->tv_sec * 1000000000;
}

/*
 * In struct scm_timestamping
 * ts[0]: software timestamp
 * ts[1]: network adapter time
 * ts[2]: hardware timestamp
 * See details in:
 * <https://www.kernel.org/doc/Documentation/networking/
 *  timestamping.txt>
 */
static inline struct scm_timestamping*
get_timestamp_from_msg(struct msghdr *msg)
{
	/* temporary pointer to parse control messages */
	struct cmsghdr *cmsg;

	/* check if the received packet contains the timestamp */
	for_each_cmsg (msg, cmsg) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SO_TIMESTAMPING) {
			return (void*) CMSG_DATA(cmsg);
			/* at the moment we'll exit loop here */
			break;
		}
	}

	/* timestamp not found */
	return NULL;
}
