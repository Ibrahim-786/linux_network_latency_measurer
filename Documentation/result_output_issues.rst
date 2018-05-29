====================
Result output issues
====================

:Date: 2018-05-24

This document will cover the WRITE_IN_SENDER preprocessor
option.


Normal behavior
===============

The measurer's normal behavior (without WRITE_IN_SENDER)
is send the result to the writer in receiver, when
receiving the packet. This way makes the output happens as
soon as possible.

The send history supports packets arriving out of order.
However, when a packet arrives out of order it's output
out of order too. This is a problem if the user wants to
make a graph, for example.


WRITE_IN_SENDER option
======================

The WRITE_IN_SENDER option makes the sender (instead of
receiver) send the packets to the writer just before
they're overwritten in the send history, ensuring that the
packets' timeout has elapsed and the output will be
ordered.

This creates a problem, though. The output gets delayed to
the amount described in section below. The bigger the send
history, the delayed the output.


The delay of the output
-----------------------

The send history has its size calculated in a way that
when the oldest entry gets overwritten its timeout has
been reached. The sender wakes up every <sleep_ms>
milliseconds and writes <packet_count> entries in send
history (let's assume <packet_count> equals 1 here).

As we have mentioned earlier, the sender sends the result
of a given entry to the writer at the time it overwrites
it. The first overwrite happens just after the buffer gets
full, and after the first write there may be **at most**
<buffer_size> entries to be output.

The output delay is defined by <sleep_ms> times the total
entries in send history.


Conclusion
==========

At the moment, we have not implemented any solution to
circumvent the problem of waiting the whole buffer to be
overwritten before sending the result to the writer.

By the way, even if we print the results in receiver while
packets arrive in order, when one packet misses there is no
solution other than block output and wait for the packet
until its timeout elapses.
