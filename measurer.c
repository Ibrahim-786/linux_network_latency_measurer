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
 * 22/09/2017, 14/10/2017, 07/05/2018
 *
 * set up, argument parsing and the main function
 */

/* link with -pthread */

#define _GNU_SOURCE /* pipe2() */

#include <arpa/inet.h> /* htons() */
#include <fcntl.h> /* O_NONBLOCK */
#include <netinet/in.h> /* inet_network() */
#include <poll.h> /* POLL* */
#include <pthread.h> /* pthread_*() */
#include <signal.h> /* SIG_BLOCK */
#include <stdint.h> /* int*_t */
#include <stdio.h> /* printf() */
#include <stdlib.h> /* atoi() */
#include <string.h> /* strcmp() */
#include <sys/socket.h> /* bind() */
#include <sys/types.h> /* bind() */
#include <unistd.h> /* getopt() close() pipe() */

#include <linux/net_tstamp.h> /* timestamp stuff */

#include "result_buffer.h"
#include "send_history.h"

#include "writer.h"
#include "receiver.h"
#include "storer.h"
#include "sender.h"

#include "thread_context.h"

#include "multi_thread.h"
#include "single_thread.h"

struct measurer {
	/* config */
	uint32_t addr;
	uint16_t port;
	unsigned int sleep_ms;
	unsigned int packet_count;
	unsigned int max_latency;
	unsigned int result_buffering_size;
	int is_multi_thread; /* boolean */
#ifdef SEND_COUNT
	int n_to_send;
#endif
	int output_type;
	char *writer_file;


	/* send and receive socket */
	int send_sfd;
	int recv_sfd;

	/* The sender, storer and receiver use it */
	struct send_history send_history;

	/* used to communicate to the writer */
	struct result_buffer result_buffer;

	/* writer thread and its file descriptor */
	struct thread_ctx writer_thread;

	struct writer   writer;
	struct receiver receiver;
	struct storer   storer;
	struct sender   sender;
};


/* NOTE: not used anymore */
//volatile sig_atomic_t _keep_running = 1;


static void
print_usage(void)
{
	printf("usage: cmd [OPT] <mirror_address> <port>\n");
}

static void
print_help(void)
{
	print_usage();
	printf(
"  -h Print this help."
"  -b <buffering_size> Number of entries to store before writing in file\n"
#ifdef SEND_COUNT
"  -c <packets_to_send> Number of packets to send before exit.\n"
"     Default: unlimited.\n"
#endif
"  -f [bin|csv|friendly (default)] Output type.\n"
"     Friendly, binary, comma separated values.\n"
"  -i <sleep_ms> (in milliseconds) Interval for sending packets.\n"
"  -n <packet_count> Number of packets to send after every interval.\n"
"  -o <output_file> File to write measurements (default stdout).\n"
"  -t Enable multi thread mode\n"
"  -W <timeout> (in milliseconds) Maximum latency allowed for packets.\n"
	);
}

static int
block_all_signals(void)
{
	sigset_t signal_set;

	sigfillset(&signal_set);
	/* NOTE: pthread_sigmask returns error numbers */
	if (pthread_sigmask(SIG_BLOCK, &signal_set, NULL) != 0)
		return -1;

	return 0;
}

static void
prepare_address(struct sockaddr_in *saddr, uint32_t addr, uint16_t port)
{
	saddr->sin_family = AF_INET;
	/* host-to-network-(short/long): convert byte order */
	saddr->sin_port = htons(port);
	/* set mirror address as default for send operation */
	saddr->sin_addr.s_addr = htonl(addr);
}

/*
 * SO_SELECT_ERR_QUEUE: wake the socket with `POLLPRI|POLLERR`
 * if it is in the error list, which would enable software to
 * wait on error queue packets without waking up for regular
 * data on the socket.
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
set_timestamp_opt(int fd, unsigned int opt)
{
	/* set timestamp option */
	return setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, (char*) &opt,
	                  sizeof(opt));
}

static void
cleanup_network(struct measurer *m)
{
	close(m->send_sfd);
	close(m->recv_sfd);
}

static int
setup_network(struct measurer *m)
{
	struct sockaddr_in recv_bind_addr;

	/*
	 * open receive socket
	 * ===================
	 *
	 * It's necessary a separate socket to receive,
	 * otherwise it keeps waking up with POLLERR.
	 * Also, sometimes it wakes up with POLLPRI
	 * (see set_pollpri_on_errqueue()).
	 * TODO: investigate this!
	 */
	m->recv_sfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	if (m->recv_sfd == -1)
		return -1;

	/* bind receive socket */
	prepare_address(&recv_bind_addr, INADDR_ANY, m->port);
	if (bind(m->recv_sfd, (struct sockaddr*) &recv_bind_addr,
	         sizeof(recv_bind_addr)) == -1)
		goto _go_close_recv_socket;

	/* TODO: allow user choose which type of timestamp he wants */
	if (set_timestamp_opt(m->recv_sfd,
	                      SOF_TIMESTAMPING_SOFTWARE |
	                      SOF_TIMESTAMPING_RX_SOFTWARE) == -1)
		goto _go_close_recv_socket;

	/*
	 * open send socket
	 * ================
	 */

	m->send_sfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	if (m->send_sfd == -1)
		goto _go_close_recv_socket;

	/*
	 * allow to wake up only when data (timestamp in
	 * this case) arrives in error queue
	 */
	if (set_pollpri_on_errqueue(m->send_sfd) == -1)
		goto _go_close_send_socket;

	/* TODO: allow user choose which type of timestamp he wants */
	if (set_timestamp_opt(m->send_sfd,
	                      SOF_TIMESTAMPING_SOFTWARE |
	                      SOF_TIMESTAMPING_OPT_CMSG |
	                      SOF_TIMESTAMPING_TX_SCHED) == -1)
		goto _go_close_send_socket;

	return 0;

_go_close_send_socket:
	close(m->send_sfd);
_go_close_recv_socket:
	close(m->recv_sfd);
	return -1;
}

static unsigned int
calculate_send_history_buffer_size(struct measurer *m)
{
	unsigned int entries_to_keep;

	/*
	 * NOTE: maximum latency doesn't necessarily
	 * need to be a multiple of sleep_ms
	 */

	/*
	 * Calculate the number of wakeups of the sender
	 * that happen until a packet expire.
	 *   i.e. How many times sleep_ms fits in
	 *   max_latency
	 */
	entries_to_keep = m->max_latency / m->sleep_ms;
	/*
	 * we add one here because there may be some
	 * remainder of the division
	 */
	entries_to_keep += 1;

	/*
	 * multiply by the number of packets sent in each
	 * wakeup
	 */
	return entries_to_keep * m->packet_count;
}

static void
cleanup_send_history(struct measurer *m)
{
	free(m->send_history.buffer);
	pthread_mutex_destroy(&m->send_history.mtx);
}

static int
setup_send_history(struct measurer *m)
{
	unsigned int buffer_size;

	/* mutex lock to manage send history access */
	if (pthread_mutex_init(&m->send_history.mtx, NULL) != 0)
		return -1;

	/*
	 * The ring buffer's minimum size must have room
	 * for at least elements that arrive at maximum
	 * allowed latency.
	 *
	 * when we reach the end, the entries in the
	 * beginning should be expired.
	 */
	buffer_size = calculate_send_history_buffer_size(m);
	m->send_history.buffer = calloc(buffer_size,
	                                sizeof(struct sent_packet));
	m->send_history.control.size = buffer_size;
	single_ring_buffer_reset(&m->send_history.control);

	/*
	 * calculate the boundary for packet id
	 *
	 * The packet_id must wrap at a multiple of buffer
	 * size. It's calculated the first value below
	 * PACKET_ID_MAX that is multiple of buffer_size.
	 *
	 * (PACKET_ID_MAX / buffer_size) results in the
	 * number of times buffer_size can fit in
	 * PACKET_ID_MAX.
	 *
	 * NOTE: the multiplication doesn't necessarily
	 * cancel the division, given that it's a integer
	 * division
	 */
	m->send_history.packet_id_boundary =
	  (PACKET_ID_MAX / buffer_size) * buffer_size;

	return 0;
}

static void
cleanup_result_buffer(struct measurer *m)
{
	struct result_buffer *b = &m->result_buffer;

	free(b->buffer);
	close(b->readfd);
	close(b->writefd);
}

static int
setup_result_buffer(struct measurer *m)
{
	struct result_buffer *b = &m->result_buffer;
	int tmp[2];

	if (pipe2(tmp, O_NONBLOCK) == -1)
		return -1;

	b->readfd = tmp[0];
	b->writefd = tmp[1];

	/*
	 * initialize the result buffer and its variables
	 *
	 * NOTE: If the results are sent by the sender,
	 * it's better to make the buffer size a multiple
	 * of packet_count.
	 */
	b->size = m->result_buffering_size * sizeof(*b->buffer);
	b->buffer = malloc(b->size);
	b->boundary = m->result_buffering_size;
	b->index = 0;

	/*
	 * log the number of entries that couldn't be
	 * transferred to the writer
	 */
	b->misses = 0;

	return 0;
}

static void
cleanup_measurer(struct measurer *m)
{
	sender_cleanup(&m->sender);
	storer_cleanup(&m->storer);
	receiver_cleanup(&m->receiver);
	writer_cleanup(&m->writer);
	thread_context_cleanup(&m->writer_thread);
	cleanup_result_buffer(m);
	cleanup_send_history(m);
	cleanup_network(m);
}

static int
setup_measurer(struct measurer *m)
{
	/*
	 * common setup
	 * ============
	 */

	if (setup_network(m) == -1)
		return -1;

	if (setup_send_history(m) == -1)
		goto _go_close_network;

	if (setup_result_buffer(m) == -1)
		goto _go_cleanup_send_history;

	/*
	 * elements setup
	 * ==============
	 */

	/* writer thread */
	if (thread_context_setup(&m->writer_thread, (void*) writer_do_its_job,
	    &m->writer, m->result_buffer.readfd, POLLIN) == -1)
		goto _go_cleanup_result_buffer;

	/* writer */
	m->writer.result_buffer = &m->result_buffer;
	m->writer.output_type = m->output_type;
	if (writer_setup(&m->writer, m->writer_file) == -1)
		goto _go_writer_thread_cleanup;

	/* receiver */
	m->receiver.result_buffer = &m->result_buffer;
	m->receiver.send_history =  &m->send_history;
	m->receiver.sfd = m->recv_sfd;
	if (receiver_setup(&m->receiver, m->max_latency) == -1)
		goto _go_writer_cleanup;

	/* storer */
	m->storer.send_history = &m->send_history;
	m->storer.sfd = m->send_sfd;
	if (storer_setup(&m->storer) == -1)
		goto _go_receiver_cleanup;

	/* sender */
#ifdef WRITE_IN_SENDER
	m->sender.result_buffer = &m->result_buffer;
#endif
	m->sender.send_history = &m->send_history;
	m->sender.sfd = m->send_sfd;
#ifdef SEND_COUNT
	m->sender.send_count = m->n_to_send;
	m->sender.max_latency = m->max_latency;
#endif
	prepare_address(&m->sender.addr, m->addr, m->port);
	if (sender_setup(&m->sender, m->sleep_ms, m->packet_count) == -1)
		goto _go_storer_cleanup;

	return 0;

//_go_sender_cleanup:
	sender_cleanup(&m->sender);
_go_storer_cleanup:
	storer_cleanup(&m->storer);
_go_receiver_cleanup:
	receiver_cleanup(&m->receiver);
_go_writer_cleanup:
	writer_cleanup(&m->writer);
_go_writer_thread_cleanup:
	thread_context_cleanup(&m->writer_thread);
_go_cleanup_result_buffer:
	cleanup_result_buffer(m);
_go_cleanup_send_history:
	cleanup_send_history(m);
_go_close_network:
	cleanup_network(m);
	return -1;
}

static int
parse_command_line_args(struct measurer *m, int argc, char **argv)
{
	int c;

	/* '+' = stop option processing when the first non-option is found */
#ifdef SEND_COUNT
	while ((c = getopt(argc, argv, "+b:c:f:i:n:o:thW:")) != -1) {
#else
	while ((c = getopt(argc, argv, "+b:f:i:n:o:thW:")) != -1) {
#endif
		switch (c) {
		case 'b':
			m->result_buffering_size = atoi(optarg);
			break;
#ifdef SEND_COUNT
		case 'c':
			if ((m->n_to_send = atoi(optarg)) <= 0)
				m->n_to_send = -1;
			break;
#endif
		case 'f':
			if (strcmp(optarg, "bin") == 0)
				m->output_type = WRITER_OUTPUT_BINARY;
			else if (strcmp(optarg, "csv") == 0)
				m->output_type = WRITER_OUTPUT_CSV;
			break;
		case 'i':
			m->sleep_ms = atoi(optarg);
			break;
		case 'n':
			m->packet_count = atoi(optarg);
			break;
		case 'o':
			/* get filename where we'll write our measurements */
			m->writer_file = optarg;
			break;
		case 't':
			m->is_multi_thread = 1;
			break;
		case 'W':
			/* in milliseconds */
			m->max_latency = atoi(optarg);
			break;
		case 'h':
		default:
			print_help();
			exit(0);
		}
	}

	/* error if there are invalid option arguments */
	if (!m->result_buffering_size || !m->packet_count
	    || !m->sleep_ms || !m->max_latency) {
		printf("result_buffering_size, sleep_ms, packet_count "
		       "and max_latency cannot be zero\n");
		return -1;
	}

	/* error if mandatory arguments weren't found */
	if ((argc - optind) != 2) {
		print_usage();
		return -1;
	}

	/* get mirror address */
	m->addr = inet_network(argv[optind]);
	if (m->addr == -1) {
		printf("not a valid address\n");
		return -1;
	}

	/* get mirror port */
	m->port = atoi(argv[optind+1]);

	return 0;
}

static void
set_default_args(struct measurer *m)
{
	/* defaults */
	m->sleep_ms = 1000;
	m->packet_count = 1;
	m->max_latency = 500;
	m->result_buffering_size = 1;
	m->is_multi_thread = 0;
#ifdef SEND_COUNT
	m->n_to_send = -1;
#endif
	m->output_type = WRITER_OUTPUT_FRIENDLY;
	/* NULL defaults to standard output */
	m->writer_file = NULL;
}

#define set_and_goto(var, val, label) \
	do { \
		var = val; \
		goto label; \
	} while (0)

int
main(int argc, char **argv)
{
	int ret = 0;
	struct measurer m;
	struct measurer_elements elements;

	/* we catch signals in the run loop */
	if (block_all_signals() == -1)
		return 1;

	set_default_args(&m);
	if (parse_command_line_args(&m, argc, argv) == -1)
		return 1;

	if (setup_measurer(&m) == -1)
		return 1;

	/* print configuration information */
	printf("result buffering size: %d\n"
	       "packet count: %d\n"
	       "send interval (sleep time): %d milliseconds\n"
	       "maximum allowed latency: %d milliseconds\n"
	       "output file: %s\n",
	       m.result_buffering_size, m.packet_count, m.sleep_ms,
	       m.max_latency, m.writer_file ? m.writer_file : "stdout");

	/*
	 * start writer thread
	 *
	 * TODO: implement writer using the thread_ctx
	 * structure from multi_thread.c
	 */
	if (thread_start(&m.writer_thread) == -1)
		set_and_goto(ret, 1, _go_cleanup_measurer);

	/*
	 * multi thread mode: Run each step in a separated
	 * thread.
	 *
	 * single thread mode: Run all steps except writer
	 * in a single thread by polling all file
	 * descriptors.
	 */

	elements.sender =   &m.sender;
	elements.storer =   &m.storer;
	elements.receiver = &m.receiver;

	if (m.is_multi_thread)
		ret = multithread_run(&elements);
	else
		ret = singlethread_run(&elements);

	if (thread_terminate(&m.writer_thread))
		ret = 1;

#ifdef WRITE_IN_SENDER
	printf("Flushing send history\n");
	sender_flush_send_history(&m.sender);
	writer_do_its_job(&m.writer);
#endif

	printf("Exiting\n");

	/* display some metrics */
	printf("%ld packets sent\n", m.sender.total_packets_sent);
	printf("%ld timestamps stored\n", m.storer.total_packets_stored);
	printf("%ld packets received\n", m.receiver.valid_packets);
	if (m.receiver.valid_packets) {
		printf("average round trip latency: %ld.%06ld ms\n",
		       m.receiver.nsec_sum /
		         m.receiver.valid_packets / 1000000,
		       m.receiver.nsec_sum /
		         m.receiver.valid_packets % 1000000);
	}

_go_cleanup_measurer:
	cleanup_measurer(&m);
	return ret;
}
