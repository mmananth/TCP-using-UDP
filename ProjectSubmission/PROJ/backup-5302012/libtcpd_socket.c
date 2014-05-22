#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "tcpd_socket.h"

#define TCPD_M2_PORT_IN 1051
#define TCPD_M1_PORT_OUT 1055
#define TRUE 1
#define FALSE 0
#define LOCAL_IP "127.0.0.1"
#define END_MESSAGE "##$$CONNECTEND$$##"

/*START:For TCPDM2*/
extern struct sockaddr_in TCPDM2_addr;
/*END:For TCPDM2*/

/*START:For TCPDM1*/
extern struct sockaddr_in TCPDM1_addr;
char is_server=FALSE;
/*END:For TCPDM1*/

int SOCKET(int domain, int type, int protocol){
	return socket(domain, SOCK_DGRAM, protocol);
}

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
	struct sockaddr_in temp_addr;
	is_server = TRUE;
	memcpy(&temp_addr,addr,sizeof(struct sockaddr));
	temp_addr.sin_port=TCPD_M1_PORT_OUT;
	return bind(sockfd, (struct sockaddr *)&temp_addr, addrlen);
}

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
	fd_set read_set;
	FD_ZERO(&read_set);
	FD_SET(sockfd,&read_set);
	if (select(sockfd+1, &read_set, NULL, NULL, NULL) == -1)
		return -1;
	return sockfd;
}

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	/*START: if this is the first time the function is being called end establish connection with TCPD
	 * and send destination details*/
	TCPDM2_addr.sin_family = AF_INET;            			// Address family to use
	TCPDM2_addr.sin_port = htons(TCPD_M2_PORT_IN);    // Port number to use
	TCPDM2_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);

	if(sendto(sockfd, addr, sizeof(struct sockaddr), 0,
			(struct sockaddr *)&TCPDM2_addr, sizeof(TCPDM2_addr))==-1)
	{
		perror("Cannot send data from SEND to TCPD_M2\n");
		return -1;
	}
	return 0;
}

ssize_t SEND(int sockfd, const void *buf, size_t len, int flags) {
	/*START:Send the data from buffer through UDP to TCPD_M2*/
	sleep(1);
	return(sendto(sockfd, buf, len, flags, (struct sockaddr *) &TCPDM2_addr,
			sizeof(struct sockaddr_in)));
	/*END:Send the data from buffer through UDP to TCPD_M2*/
}

ssize_t RECV(int sockfd, void *buf, size_t len, int flags) {
	socklen_t address_len;

	/*START: Receive the data from TCPD_M1*/
	address_len = sizeof(struct sockaddr_in);
	return(recvfrom(sockfd, buf, len, 0,
			(struct sockaddr *)&TCPDM1_addr, &address_len));
	/*END: Receive the data from TCPD_M1*/
}

int CLOSE(int sockfd)
{
	char out_buf[20];
	strcpy(out_buf,END_MESSAGE);

	if (is_server==TRUE)
		return (close(sockfd));
	else
	{
		/*START:send END message to TCPD_M2*/
		if(sendto(sockfd, out_buf, strlen(END_MESSAGE)+1, 0,
				(struct sockaddr *) &TCPDM2_addr,	sizeof(struct sockaddr_in))==-1)
			return 1;
		else
			return (close(sockfd));
		/*END:send END message to TCPD_M2*/
	}
}


