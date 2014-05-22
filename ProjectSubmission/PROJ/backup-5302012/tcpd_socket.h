#include <sys/socket.h>
#include <arpa/inet.h>
#define END_MESSAGE "##$$CONNECTEND$$##"

/*START:For TCPDM2*/
struct sockaddr_in TCPDM2_addr;
/*END:For TCPDM2*/

/*START:For TCPDM1*/
struct sockaddr_in TCPDM1_addr;
/*END:For TCPDM1*/

int SOCKET(int domain, int type, int protocol);

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

ssize_t SEND(int sockfd, const void *buf, size_t len, int flags);

ssize_t RECV(int sockfd, void *buf, size_t len, int flags);

int CLOSE(int sockfd);
