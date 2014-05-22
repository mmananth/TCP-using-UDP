#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define TCPD_M2_PORT_IN 1051
#define TCPD_M1_PORT_OUT 1055

/*START:For TCPDM2*/
struct sockaddr_in dest;
struct sockaddr_in TCPDM2_addr;
unsigned int TCPDM2_socket;
char TCPDM2_IP[16];
/*END:For TCPDM2*/

/*START:For TCPDM1*/
char TCPDM1_IP[16];
unsigned int TCPDM1_socket;
struct sockaddr_in TCPDM1_addr;
/*END:For TCPDM1*/

socklen_t address_len;
struct in_addr addr;


int SOCKET(int domain, int type, int protocol){

}

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen){

}

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen){

}

int CONNECT(){
	return 0;
}

ssize_t SEND(int sockfd, const void *buf, size_t len, int flags) {
	static int call = 0;
	int ret;
	socklen_t address_len;
	struct in_addr addr;

	/*START: if this is the first time the function is being called end establish connection with TCPD
	 * and send destination details*/
	if (call == 0) {
		TCPDM2_socket = socket(AF_INET, SOCK_DGRAM, 0);
		TCPDM2_addr.sin_family = AF_INET;            			// Address family to use
		TCPDM2_addr.sin_port = htons(TCPD_M2_PORT_IN);    // Port number to use
		TCPDM2_addr.sin_addr.s_addr = inet_addr(TCPDM2_IP);
		address_len = sizeof(dest);
		/*if(getpeername(sockfd, (struct sockaddr *)&dest, &address_len)!=0)
		{
			perror("Peer details not retrieved\n");
			return -1;
		}*/
		addr.s_addr = dest.sin_addr.s_addr;
		printf("The peer address is %s",inet_ntoa(addr));
		dest.sin_port = htons(TCPD_M1_PORT_OUT);
		if(sendto(TCPDM2_socket, &(dest), address_len, 0,
				(struct sockaddr *) &TCPDM2_addr, sizeof(TCPDM2_addr))==-1)
		{
			perror("Cannot send data from SEND to TCPD_M2\n");
			return -1;
		}
		addr.s_addr = TCPDM2_addr.sin_addr.s_addr;
		printf("Sent %u size data from ftpc to %s\n",address_len,inet_ntoa(addr));
		call++;
	}
	/*END: if this is the first time the function is being called end establish connection with TCPD*/

	/*START:Send the data from buffer through UDP to TCPD_M2*/
	sleep(1);
	sendto(TCPDM2_socket, buf, len, flags, (struct sockaddr *) &TCPDM2_addr,
			sizeof(TCPDM2_addr));  				//send the data  the metadata of the file
	addr.s_addr = TCPDM2_addr.sin_addr.s_addr;
	printf("Sent %u size data from ftpc to %s\n",len,inet_ntoa(addr));
	/*END:Send the data from buffer through UDP to TCPD_M2*/
}

ssize_t RECV(int sockfd, void *buf, size_t len, int flags) {
	static int call1 = 0;
	int ret;
	unsigned int bytes_recv_troll = 0;
	struct sockaddr_in TCPDM1_addr_in;
	socklen_t address_len;
	struct in_addr addr;
	/*START: if this is the first time the function is being called end establish connection with TCPD*/
	if (call1 == 0) {
		TCPDM1_socket = socket(AF_INET, SOCK_DGRAM, 0);
		TCPDM1_addr.sin_family = AF_INET;            			// Address family to use
		TCPDM1_addr.sin_port = htons(TCPD_M1_PORT_OUT);   // Port number to use
		TCPDM1_addr.sin_addr.s_addr = htonl(INADDR_ANY);  	// Send to passed IP address
		address_len = sizeof(TCPDM1_addr);
		if(bind(TCPDM1_socket, (struct sockaddr *)&TCPDM1_addr, sizeof(TCPDM1_addr))!=0)
		{
			perror("Binding failure RECV\n");
			return -1;
		}
		call1++;
	}
	/*END: if this is the first time the function is being called end establish connection with TCPD*/

	/*START: Receive the data from TCPD_M1*/
	bytes_recv_troll = recvfrom(TCPDM1_socket, buf, len, 0,
			(struct sockaddr *)&TCPDM1_addr_in, &address_len);
	addr.s_addr = TCPDM1_addr_in.sin_addr.s_addr;
	printf("Rcv %u size data from %s to ftps\n",bytes_recv_troll ,inet_ntoa(addr));
	return bytes_recv_troll;
	/*END: Receive the data from TCPD_M1*/
}
