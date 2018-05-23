========================
Network latency measurer
========================

:Date: 25/09/2017, 06/10/2017, 14/10/2017

Measure round-trip latencies of isochronous sent packets.

There are two computers involved, the measurer and the
mirror. The measurer sends packets to the mirror, which in
turn send them back, so the former can measure the time
between send and receive the packet.


How to use
==========

Use ``-h`` option for help.


How it works
============

The measurer sends an ID to the mirror and locally stores
the send timestamp. The mirror receives the ID and send it
back. The measurer receives the ID from mirror, calculates
the difference between send and receive timestamps, and
write the result in a file.

There is a ring buffer (``send_history.h``) to keep track
of the IDs and send timestamps. Its size is calculated in
a way that when it gets full the ID corresponding to the
oldest entry has been reached its timeout (or it's just
about to reach it). The ring buffer overwrites.

To get the entry that corresponds to a given ID from ring
buffer, just calculate the remainder:
``ID % ring_buffer_size``. As a consequence, the maximum
value for ID (where it wraps) is always a multiple of
ring_buffer_size.

Before sending the results to the writer, which will write
them to the file, there is a buffering (settable by user
using ``-b`` option) done by ``result_buffer_insert_entry()``
in ``result_buffer.h``. When the buffer gets full, it is
transferred to the writer using a pipe.

There are four steps to complete a measurement:

1. Sender: Put the current ID in the next entry of ring
   buffer and send the ID to the mirror, which will send
   it back.
2. Storer: Wait for send timestamp (the packet may take
   some time until be sent), and put it on the
   corresponding entry of ring buffer.
3. Receiver: Receive packet from mirror, get the send
   timestamp from ring buffer, and send the result to
   the writer.
4. Writer: Write the results (latencies) to a file.


Implementation FAQ
==================

**Why setting SO_SELECT_ERR_QUEUE socket option?**
The option allows poll wake up when data is available in
error queue.

**Why are two sockets used, one to receive and another to
send?**
One socket is used to send the packets and get their
timestamps. The other is used to receive packets from
mirror. It was done this way because when receiving from
the same socket that send and return timestamps, many
unexpected wake ups happened with poll() returning POLLERR
and sometimes POLLPRI (possibly because of
SO_SELECT_ERR_QUEUE).

**Why is the ring buffer the way it is?**
Because it fits exactly the purpose of the program. It
allows packets arriving out of order, detecting duplicates,
and the user to define a maximum latency for them.


Writer file format
==================

Friendly (default)::

	<packet_id> <diff_in_milliseconds> ms

CSV::

	<packet_id>, <diff_in_microseconds>

Binary::

	uint_64 (packet_id)
	uint_64 (diff_in_microseconds)
	...
