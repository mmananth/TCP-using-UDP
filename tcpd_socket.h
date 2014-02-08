/****************************************************************************************************************************************
 * Authors	: Abhishek and Ananth
 * Date		: June-6-2012
 ****************************************************************************************************************************************
 * File				: TCPD_socket.c
 * Description		:
 * Calling format	:
 * Return value		:
 ****************************************************************************************************************************************/

/****************************************************
 * Header files
 ***************************************************/
#include <sys/socket.h>						// Internet protocol family. Defines the sockaddr structure, SOCK_DGRAM/SOCK_STREAM
#include <arpa/inet.h>						// Contains the definitions for the internet operations (htonl, htons, ntohl, ntohs)
#define END_MESSAGE "##$$CONNECTEND$$##"	// Signals the end of the transmission

/*START:For TCPDM2*/
struct sockaddr_in TCPDM2_addr;
/*END:For TCPDM2*/

/*START:For TCPDM1*/
struct sockaddr_in TCPDM1_addr;
/*END:For TCPDM1*/

int SOCKET(int domain, int type, int protocol);								// Creates a socket

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen);		// Binds the socket to the address

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen);			// Accepts a connection from an address

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen);	// sends the destination details to the address specified

ssize_t SEND(int sockfd, const void *buf, size_t len, int flags);			// Send the data from buffer through UDP to TCPD_M2

ssize_t RECV(int sockfd, void *buf, size_t len, int flags);					// Receive the data from TCPD_M1

int CLOSE(int sockfd);														// send END message to TCPD_M2
