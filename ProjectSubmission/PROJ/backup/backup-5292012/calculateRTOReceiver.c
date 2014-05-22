#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

/* 1. create a socket.
 * 2. initialize the IP address and port
 * 3. bind the socket to the address
 * 5. receive the character sent by the sender
 * 6. send the ack back	*/

#define RCVR_PORT 13000
#define SEND_PORT 12000
#define PACKET_COUNT 1
#define BUFFER_SIZE 100

struct packet
{
	int pktID;
	char buf[BUFFER_SIZE];
};

int main()
{
	struct packet pktRecv;
	int sockSend,sockRecv;
	int setSockOpt = 1;
	int recvCount = 0;
	struct sockaddr_in rcvrAddr, sendAddr;
	/*char buf[BUFFER_SIZE];*/
	int loopIndex = 0;

	/* create the socket */
	sockSend = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockSend < 0)
	{
		perror("Error while creating the socket receiver");
		return(1);
	}

	sockRecv = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockRecv < 0)
	{
		perror("Error while creating the socket receiver");
		return(1);
	}

	rcvrAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	rcvrAddr.sin_family = AF_INET;
	rcvrAddr.sin_port = htons(RCVR_PORT);			/*sender sends on 13000 and receiver listens on 13000*/

	sendAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	sendAddr.sin_family = AF_INET;
	sendAddr.sin_port = htons(SEND_PORT);			/*receiver sends back on 12000*/

	if(setsockopt(sockRecv, SOL_SOCKET, SO_REUSEADDR, &setSockOpt, sizeof(int)) < 0)
	{
		perror("Error while using setsockopt");
		return(1);
	}

	if(bind(sockRecv,(struct sockaddr *)&rcvrAddr, sizeof(rcvrAddr)) < 0)
	{
		perror("Error while binding socket to address\n");
		return(1);
	}

	int addrLen = sizeof(rcvrAddr);
	while(1)
	{
		printf("Inside recv\n");
		if(recvfrom(sockRecv, &pktRecv, sizeof(pktRecv), 0, (struct sockaddr *)&rcvrAddr, &addrLen) < 0)
		{
			perror("Error while receiving data\n");
			return(1);
		}
		recvCount++;

		/*if(strcmp(buf, EOFL)==0)
		{
			printf("Received the EOFL\n");
			break;
		}*/

		if(sendto(sockSend, &pktRecv, sizeof(pktRecv), 0, (struct sockaddr *)&sendAddr, sizeof(sendAddr)) < 0)
		{
			perror("Error while sending data\n");
			return(1);
		}
		if(recvCount == PACKET_COUNT)
		{
			break;
		}
	}

	for(loopIndex = 0; loopIndex < PACKET_COUNT; loopIndex++)
	{
		printf("The received packet ID is: %d\n", pktRecv.pktID);
		printf("The received data is: %c\n", pktRecv.buf[loopIndex]);
	}
}
