#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

/*1. create a socket.
 * 2. initialize the IP address and port
 * 3. bind the socket to the address
 * 4. start the timer
 * 5. send the character to the receiver
 * 6. receive the ack back
 * 7. stop the time
 * 8. convert the usec to sec. divide by 1,000,000
 * 9. take the avg of the RTT*/

/* Refer to - https://tools.ietf.org/html/rfc2988 - for the algorithm details */
/* http://www.koders.com/c/fid625ABA27F7C0D13BEC2615E61CC2D29C3E1EBA7C.aspx?s=IPv6 */

#define SEND_PORT 13000
#define RCVR_PORT 12000
#define PACKET_COUNT 1
#define BUFFER_SIZE 100
#define AVG 1000.0

/*
 Err = M - A;
 A = A + g*Err;
 D = D + h (abs(Err) - D);
 RTO = A + 4D;

 A - smoothed RTT (an estimator of the avg)
 M - is the RTT (calculated after receiving every packet)
 D - smoothed mean deviation
 */

const float g = 0.125;
const float h = 0.25;

float Err, M, A = 0;	/*Initial value of A has to be 0*/
float D = 3;			/*Initial value has to be 3*/
int RTO = 6;			/*RTO is initially set to 6*/
static int rttCount = 0;

struct packet
{
	int pktID;
	char buf[BUFFER_SIZE];
};

/*This is only for the initial SYN packet*/
int calculateInitialRTO(int index)
{
	/*For the very first packet. RTO = 6*/
	if(index == 0){
		RTO = A + 2*D;
	}
	/*For first timeout. RTO = 12*/
	else if(index == 1){
		RTO = A + 4*D;
	}
	/*For second timeout. RTO = 24*/
	else if(index == 2){
		RTO = A + 8*D;
	}
	/*For third timeout. RTO = 48*/
	else if(index == 3){
		RTO = A + 16*D;
	}
	return RTO;
}

/*This is for packets after the initial SYN. Only segments containing data are ACKed*/
int calculateRTO(float M)
{
	Err = M - A;
	A = A + g*Err;
	D = D + h * (abs(Err) - D);
	RTO = A + 4*D;
	return RTO;
}

int main()
{
	struct packet pktSend, pktRecv;
	char toSend='a';
	struct timeval start, end;
	struct sockaddr_in send_addr, rcvr_addr;
	unsigned int sockSend, sockRecv;
	int setSockOpt = 1;				/*For setsockopt*/
	double t1,t2;
	int numPktSent = 0, reTransmit = 0;
	int addrLen;
	char recvBuf[BUFFER_SIZE];

	int reTxCount = 0;					/*To keep count of the retransmissions */

	/*static int rttCount = 0;
	const int G = 1;
	const int K = 4;
	double srtt = 0.0, rttvar = 0.0, rto = 3.0;
	double rtt = 0.0;*/



	/* srtt - Smoothed Round-Trip Time
	 * rttvar - Round-Trip Time Variation
	 * rto - Retransmission TimeOut. The initial value of RTO should be 3
	 * rtt - Round Trip Time */

	memcpy(pktSend.buf, &toSend, sizeof(toSend));

	sockSend = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockSend < 0)
	{
		perror("Error while creating socket - sockSend\n");
		return(1);
	}

	sockRecv = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockRecv < 0)
	{
		perror("Error while creating socket - sockRecv\n");
		return(1);
	}

	send_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(SEND_PORT);		/*sender sends on 13000 and receiver listens on 13000*/

	rcvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	rcvr_addr.sin_family = AF_INET;
	rcvr_addr.sin_port = htons(RCVR_PORT);		/*receives data on 12000*/

	if(setsockopt(sockSend, SOL_SOCKET, SO_REUSEADDR, &setSockOpt, sizeof(int)) < 0)
	{
		perror("Error while using setsockopt\n");
		return(1);
	}

	if(bind(sockRecv,(struct sockaddr *)&rcvr_addr, sizeof(rcvr_addr)) < 0)
	{
		perror("Error while binding socket to address\n");
		return(1);
	}

	t1=0.0; t2=0.0;
	addrLen=sizeof(rcvr_addr);
	/*send 100 packets to receiver and get back the ack*/
	reTxCount = 0;
	RTO = calculateInitialRTO(reTxCount);
	for(numPktSent=0; numPktSent < PACKET_COUNT; numPktSent++)
	{
		if(reTransmit != numPktSent)
		{
			reTransmit = numPktSent;
			pktSend.pktID = numPktSent;
			if(gettimeofday(&start,NULL))
			{
				printf("Start time failed\n");
				return(1);
			}
			printf("send started-%d\n", numPktSent);

			if(sendto(sockSend, &pktSend, sizeof(pktSend),0,(struct sockaddr *)&send_addr, sizeof(send_addr)) < 0)
			{
				perror("Error while sending to the receiver\n");
				return(1);
			}
			if(recvfrom(sockRecv, &pktRecv, sizeof(pktRecv), 0, (struct sockaddr *)&rcvr_addr, &addrLen) < 0 )
			{
				perror("Error in recvfrom");
				return(1);
			}

			if(pktRecv.pktID != numPktSent)
			{
				printf("Did not receive the packetID that was sent\n");
				break;
			}
		}
	}
	/*if(sendto(sockSend, &EOFL, sizeof(EOFL),0,(struct sockaddr *)&send_addr, sizeof(send_addr)) < 0)
	{
		perror("Error while sending EOFL to the receiver\n");
		return(1);
	}
	printf("sent EOFL %s\n", EOFL);*/

	/*while(1)
	{
		printf("Inside recv\n");
		if(recvfrom(sockRecv, &buf, sizeof(char), 0, (struct sockaddr *)&rcvr_addr, &addrLen) < 0 )
		{
			perror("Error in recvfrom");
			return(1);
		}
	}*/

	if(gettimeofday(&end, NULL))
	{
		printf("End time failed\n");
		return(1);
	}

	t1 += start.tv_sec+(start.tv_usec/100000);
	t2 += end.tv_sec+(end.tv_usec/100000);

	printf("start.tv_sec: %d\n", start.tv_sec);
	printf("start.tv_usec: %d\n", start.tv_usec);
	printf("end.tv_sec: %d\n", end.tv_sec);
	printf("end.tv_usec: %d\n", end.tv_usec);
	printf("t1: %lf\n", t1);
	printf("t2: %lf\n", t2);

	/* calculate mean RTT and RTO*/
	if(start.tv_sec == end.tv_sec)
	{
		rttCount++;
		M = (end.tv_usec-start.tv_usec)/AVG;
		printf("Mean RTT is: %f\n", M);
	}
	else
	{
		rttCount++;
		int sec = end.tv_sec - start.tv_sec;
		int usec = (end.tv_usec - start.tv_usec)/AVG;
		M = sec + usec;
		printf("Mean RTT is: %f\n",M);
	}

	/*Calculation after receiving the first packet*/
	if(rttCount == 1)
	{
		M = M>1?M:1;
		A = M + 0.5;
		D = A/2;
		D = D=1?D:1;
		RTO = A + 4*D;
	}
	else
	{
		RTO = calculateRTO(M);
	}

	close(sockRecv);
	close(sockSend);
	return(0);
}
