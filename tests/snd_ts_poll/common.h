
#define PORT  8080

#define POLLPRI_WAKEUP_ON_ERROR_QUEUE (1 << 1)

int
do_poll(int sfd, short request_mask);

void
do_recv(int sfd);

void
do_send(int sfd);

int
open_socket(int flags);
