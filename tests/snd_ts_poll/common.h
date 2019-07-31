
#define PORT  8080

#define BIND_SOCKET                   (1 << 0)
#define POLLPRI_WAKEUP_ON_ERROR_QUEUE (1 << 1)
#define ENABLE_TX_TIMESTAMP           (1 << 2)

int
do_select(int sfd, short request_mask);

int
do_poll(int sfd, short request_mask);

void
do_recv(int sfd);

void
do_send(int sfd);

int
open_socket(int flags);
