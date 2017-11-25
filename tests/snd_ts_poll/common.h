
#define PORT  8080

int
do_poll(int sfd);

void
do_recv(int sfd);

void
do_send(int sfd);

int
open_socket(void);
