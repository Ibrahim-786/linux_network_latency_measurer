======================
Additional information
======================

:Date: 2017-11-12


Network timing
==============


NTP vs PTP
----------

> Is the accuracy you need measured in microseconds or nanoseconds?
> If the answer is yes, you want PTP (IEEE 1588).  If the answer is
> in milliseconds or seconds, then you want NTP.

https://blog.meinbergglobal.com/2013/11/22/ntp-vs-ptp-network-timing-smackdown/


Why is IEEE 1588 so accurate?
-----------------------------

> Why is IEEE 1588 so accurate? Two words: Hardware timestamping.
> That’s it, really!

http://blog.meinbergglobal.com/2013/09/14/ieee-1588-accurate/


Linux internals
===============


Commit that introduced SO_SELECT_ERR_QUEUE
------------------------------------------

``SO_SELECT_ERR_QUEUE`` socket option makes ``poll()`` wake
up when data is available on error queue. As a result it
returns ``POLLPRI`` along with ``POLLERR``. The option was
added in the following commit on Linux kernel tree.

``7d4c04fc170087119727119074e72445f2bb192b``::

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


An email about polling on error queue
-------------------------------------

> Previously the poll in sk_receive would "timeout" and when it did so
> would check the ERRQUEUE for data and set POLLERR.  This meant that if
> sk_tx_timeout was set to 100 each poll would wait for 100ms rather than
> exiting immediately when ERRQUEUE data was available.
>
> Implement the SO_SELECT_ERR_QUEUE socket option that enables ERRQUEUE
> messages to be polled for under the POLLPRI flag, greatly increasing the
> number of packets per second that can be sent from linuxptp.

https://sourceforge.net/p/linuxptp/mailman/message/32992476/
