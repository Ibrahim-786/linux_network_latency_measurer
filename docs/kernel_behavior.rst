==========================
kernel behavior side notes
==========================

:Date: 2017-11-16


About the call-lists in this document
=====================================

call-list example::

	0 [file_A] foo()
	1 [file_B] bar()
	1 explode()

1. The function foo() located in file_A is called.
2. Inside foo(), the function bar() located in file_B
   is called.
3. Inside foo() again, the function explode() located
   also in file_B is called.

The call-lists go from top to bottom. The number on the
left corresponds to the function where the function call
described in the line is. The number 0 corresponds to
the function where the first function is called.

If two lines have the same number it means the functions
in these lines are called inside the same function.

If a line doesn't specify the file where the function call
is it's because the file is the same as the line above.


Sending packet
==============

Sending an UDP packet::

	0 [net/ipv4/udp.c] udp_sendmsg()
	1 udp_send_skb()
	2 [net/ipv4/ip_output.c] ip_send_skb()
	3 ip_local_out()
	4 __ip_local_out()
	5 nf_hook()

`nf_hook()` probably puts packet in send queue.


Timestamping
============

How the timestamp is generated.


Timestamp generation and poll wake up
-------------------------------------

The packet was sent by the user. See `Sending packet`_
section above.

.. Note: Are we inside an IRQ handler? How did we
   get here? Is the following statement true?

Now it's the time to packet to be sent to device::

	0 [net/core/dev.c] __dev_queue_xmit()
	1 [net/core/skbuff.c] __skb_tstamp_tx()
	2 [include/linux/timekeeping.h] ktime_get_real()
	2 [net/core/skbuff.c] __skb_complete_tx_timestamp()
	3 sock_queue_err_skb()
	4 skb_set_err_queue()
	4 [net/core/sock.c] sk_data_ready()->sock_def_readable()

If the ``SO_SELECT_ERR_QUEUE`` socket option is enabled,
``POLLPRI`` is masked along with ``POLLERR``.


Request and wait for timestamp
------------------------------

User wants to poll for the timestamp.

 - setsockopt(): [net/core/sock.c] ``sock_setsockopt()`` set the
   flags.

 - poll::

	0 [net/ipv4/udp.c] udp_poll()
	1 [net/core/datagram.c] datagram_poll()
	2 [fs/select.c] __poll_wait()

The ``udp_poll()`` function is registered in ``net/ipv4/af_inet.c``
file in ``struct proto_ops inet_dgram_ops`` structure.


Receiving message
=================

User reads the socket::

	0 [net/ipv4/udp.c] udp_recvmsg()

 - ``udp_recvmsg()`` is registered at
   ``net/ipv4/udp.c`` in
   ``struct proto udp_prot`` structure.

 - ``struct proto udp_prot`` structure is registered at
   ``net/ipv4/af_inet.c`` file in
   ``struct inet_protosw inetsw_array`` structure array.


If receiving from MSG_ERRQUEUE
------------------------------

User reads the socket's error queue::

	0 [net/ipv4/udp.c] udp_recvmsg()
	1 [net/ipv4/ip_sockglue.c] ip_recv_error()


Protocol routines registration
==============================

In ``inetsw_array`` of ``net/ipv4/af_inet.c`` the routines of
TCP/UDP/ICMP protocols are registered.


Syscall definition
==================

 - poll(): [fs/select.c]
 - recvmsg(): [net/socket.c]
