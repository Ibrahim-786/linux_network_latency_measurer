========================
Poll for send timestamps
========================

:Date: 17/11/2017


What is it?
===========

The purpose is to send packets and poll/wait for their
timestamps. In normal behavior, one thread poll for
timestamps `loop()`, and other thread send packets `main()`.

We change the behavior by:

- Sending packets in the same thread that poll for their timestamps.
  `SEND_IN_SAME_THREAD` flag.
- Using `POLLIN` in poll events. `USE_POLLIN` flag.
- Using `POLLPRI` in poll events. Involves setting a kernel option to
  "instantaneous wake up" on error queue. `USE_POLLPRI` flag. `USE_POLLIN`
  flag is ignored.


Compiling
=========

::

	$ gcc -pthread -o main common.c main.c

To add a flag (see section above) use `-D <flag>`


Behavior
========

Tests done on Linux 4.13.12 (glibc 2.24).


With `USE_POLLIN`
-----------------

The thread wakes up with `POLLERR` after every packet sent,
independently of `SEND_IN_SAME_THREAD`.


With `USE_POLLPRI`
------------------

The thread wakes up normally with `POLLPRI|POLLERR`,
independently of `SEND_IN_SAME_THREAD`.


With `POLLERR` (default)
------------------------

When `SEND_IN_SAME_THREAD` the thread wakes up normally with
`POLLERR`. Otherwise `loop()` doesn't wake up when a packet
is sent from `main()` thread.


Notes
=====


About `SO_SELECT_ERR_QUEUE`
---------------------------

| Linux kernel
| commit: ``7d4c04fc170087119727119074e72445f2bb192b``

::

	net: add option to enable error queue packets waking select
	
	Currently, when a socket receives something on the error
	queue it only wakes up the socket on select if it is in
	the "read" list, that is the socket has something to read.
	It is useful also to wake the socket if it is in the error
	list, which would enable software to wait on error queue
	packets without waking up for regular data on the socket.
	The main use case is for receiving timestamped transmit
	packets which return the timestamp to the socket via the
	error queue. This enables an application to select on the
	socket for the error queue only instead of for the regular
	traffic.

----------------------------------------

| linuxptp mailing list
| [Linuxptp-devel] [PATCH] sk: Modify poll to poll on ERRQUEUE

::

	On Fri, Oct 31, 2014 at 02:05:14PM -0500, Joe Schaack wrote:
	
	Previously the poll in `sk_receive` would "timeout" and when
	it did so would check the `ERRQUEUE` for data and set
	`POLLERR`.  This meant that if `sk_tx_timeout` was set to
	100 each poll would wait for 100ms rather than exiting
	immediately when `ERRQUEUE` data was available.
	
	Implement the `SO_SELECT_ERR_QUEUE` socket option that
	enables `ERRQUEUE` messages to be polled for under the
	`POLLPRI` flag, greatly increasing the number of packets per
	second that can be sent from linuxptp.

The rest of the message is a patch.


The `POLLIN` thing
------------------

In normal behavior (when sending from a different thread)
polling with `events = POLLIN` makes the thread wake up with
`POLLERR`. However, when `events = 0` or `events = POLLERR`
the thread simply doesn't wake up.
