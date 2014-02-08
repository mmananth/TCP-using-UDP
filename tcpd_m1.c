/****************************************************************************************************************************************
 * Authors	: Abhishek and Ananth
 * Date		: June-6-2012
 ****************************************************************************************************************************************
 * File				: TCPD_M1.c
 * Description		: This is the Daemon process on the server side. It receives data from the troll and stores it in a buffer. It sends
 * 					  and ACK for every packet received. It also starts a timer when the final FIN packet is sent to complete the
 * 					  tear-down of the connection. The packets are sent to the server process.
 * Calling format	: ./tcpd_m1
 ****************************************************************************************************************************************/

/****************************************************
 * Header files
 ***************************************************/
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>			// Internet protocol family. Defines the sockaddr structure, SOCK_DGRAM/SOCK_STREAM
#include <arpa/inet.h>			// Contains the definitions for the internet operations (htonl, htons, ntohl, ntohs)
#include <netinet/tcp.h>		// Contains definitions for TCP

/****************************************************
 * Constants declaration
 ***************************************************/
#define IN_PSEUDO_SIZE 516
#define TCP_HEADER_SIZE 20
#define TCPD_M1_PORT_IN 1054
#define TROLL_PORT_OUT 1053
#define MSS_SIZE 1000
#define TCPD_M1_PORT_OUT 1055
#define WINDOW 20
#define TRUE 1
#define FALSE 0
#define PASSED 0
#define FAILED 1
#define END_MESSAGE "##$$CONNECTEND$$##"		// Signals the end of the transmission
#define OUT_PSUEDO_SIZE 18
#define LOCAL_IP "127.0.0.1"
#define RTO 6

struct from_troll_message	troll_msg;
unsigned int         	TCPDM1_in_socket;   // Server in socket descriptor
unsigned int         	TCPDM1_out_socket;  // Server out socket descriptor
struct sockaddr_in   	TCPDM1_in_addr;     // Server1 Internet address
struct sockaddr_in 	 	TCPDM1_out_addr;
struct sockaddr_in   	troll_recv_addr;
struct sockaddr_in   	troll_send_addr;
int                  	bytes_recv_troll;

unsigned int 					fin_seq;

enum
{
	ESTABLISHED=0,
	CLOSE_WAIT,
	LAST_ACK,
	CLOSED,
}wrap_up_mode =0;
//Call Format TCPD_M1

/****************************************************
 * Data Structure declaration
 ***************************************************/
/****************************************************
 * struct for receiving data from the Troll
 ***************************************************/
struct from_troll_message
{
	struct sockaddr_in dest; //Address of the destination
	char body[MSS_SIZE + TCP_HEADER_SIZE];
};

/****************************************************
 * struct for sending data to the Troll
 ***************************************************/
struct to_troll_message
{
	struct sockaddr_in dest; //Address of the destination
	char body[TCP_HEADER_SIZE];
};

struct to_troll_message fin_msg;

/****************************************************
 * struct for the packet
 ***************************************************/
struct packet
{
	struct tcphdr hdr;
	char payload[MSS_SIZE];
};

/****************************************************
 * struct for the Circular Buffer
 ***************************************************/
struct
{
	int start, count;
	struct packet pack[WINDOW];
	unsigned int expected_ack;
	unsigned int exists [WINDOW];
}cb;

u_short compute_checksum_out(struct tcphdr *, struct sockaddr_in const * );
u_short compute_checksum_in(struct packet	*, struct sockaddr_in  const * );


void print_window()
{
	int i = 0;
	printf("expected pack(start):%d ",cb.expected_ack);
	for(i = 0; i < WINDOW; i++)
		printf("%u",cb.exists[(cb.start + i)%WINDOW]);

	printf("\n");
}

/****************************************************
 * Function		: cbInit
 * Description	: To initialize the Circular Buffer
 ***************************************************/
void cbInit()
{
	cb.start = 0;
	cb.count = 0;
	cb.expected_ack = 0;
}

/****************************************************
 * Function		: cbIsFull
 * Description	: Returns true if the CB is full
 ***************************************************/
int cbIsFull()
{
	if(cb.count>=WINDOW)
		return TRUE;
	else
		return FALSE;
}

/****************************************************
 * Function		: cbIsEmpty
 * Description	: Returns true if the CB is empty
 ***************************************************/
int cbIsEmpty()
{
	if(cb.count <= 0)
		return TRUE;
	else
		return FALSE;
}

/****************************************************
 * Function		: cbDel
 * Description	: Deletes an entry from the CB
 ***************************************************/
void cbDel()
{
	if(cbIsEmpty()==FALSE)
	{
		cb.count--;
		cb.exists[cb.start] = FALSE;
		cb.start = (cb.start + 1)%WINDOW;
	}
}

/****************************************************
 * Function		: cbWrite
 * Description	: Inserts the packet passed as argument
 * 				  into the CB
 ***************************************************/
int cbWrite(struct packet *p)
{
	int index;
	if(cbIsFull()==FALSE)
	{
		index = p->hdr.th_seq%WINDOW;
		memcpy(&cb.pack[index], p, sizeof(struct packet));
		cb.exists[index] = TRUE;
		cb.count = cb.count + 1;
		return PASSED;
	}
	else
		return FAILED;
}

/****************************************************
 * Function		: send_fin
 * Description	: Sends the FIN for the final 4-way handshake
 ***************************************************/
int send_fin()
{
	/*START: Send the fin packet*/
	if(sendto(TCPDM1_in_socket, &fin_msg, sizeof(struct to_troll_message), 0,
			(struct sockaddr *)&troll_send_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("Could not send the FIN");
		return FAILED;
	}
	/*START: Send the fin packet*/
}

/****************************************************
 * Function		: send_ack
 * Description	: Sends ACKs for the packets received
 ***************************************************/
int send_ack()
{
	struct packet			    *pkt;
	struct tcphdr 				hdr;
	struct tcphdr 				fin_hdr;
	struct to_troll_message to_troll;
	unsigned int i=0;

	/*START: Construct the packet*/
	pkt = (struct packet*)troll_msg.body;
	hdr.th_ack = pkt->hdr.th_seq+1;
	hdr.th_sport = pkt->hdr.th_dport;
	hdr.th_dport = pkt->hdr.th_sport;
	hdr.th_flags = TH_ACK;
	hdr.th_sum = compute_checksum_out(&hdr,&troll_msg.dest);
	memcpy(to_troll.body, &hdr, sizeof(hdr));
	to_troll.dest = troll_msg.dest;
	/*END: Construct the packet*/


	printf("Recvd %u size data to tcpd_m1 from %s %d\n",(bytes_recv_troll),
			inet_ntoa(troll_recv_addr.sin_addr),pkt->hdr.th_seq);

	/*START: The destination IP would be the IP of the received packets*/
	troll_send_addr.sin_addr = troll_recv_addr.sin_addr;
	/*END: The destination IP would be the IP of the received packets*/

	/*START: Send the acknowledgment to troll*/
	if(sendto(TCPDM1_in_socket, &to_troll, sizeof(to_troll), 0,
			(struct sockaddr *)&troll_send_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("Could not send the acknowledgement");
		return FAILED;
	}
	printf("send ACK for seq %d chksum %u start window %u\n",pkt->hdr.th_seq,
			hdr.th_sum,cb.expected_ack);
	fflush(stdout);
	/*END: Send the acknowledgment to troll*/

	/*START: Check If it is the FIN packet and change state*/
	if((pkt->hdr.th_flags & TH_FIN)!=0)
	{
		if(wrap_up_mode == ESTABLISHED)
		{
			printf("FIN received entering close wait\n");
			wrap_up_mode = CLOSE_WAIT;
			fin_seq = pkt->hdr.th_seq;
		}
	}
	/*END: Check If it is the FIN packet*/

	/*START: See if the data was retransmitted, if yes then no need to send it to ftps*/
	else if(pkt->hdr.th_seq>=cb.expected_ack)
	{
		/*START: Buffer the packet*/
		if(!cb.exists[(pkt->hdr.th_seq)%WINDOW])
			cbWrite(pkt);
		/*END: Buffer the packet*/

		/*START: Send all the packets that are in sequence to ftps*/
		while(cb.exists[cb.expected_ack%WINDOW] == TRUE)
		{
			/*START:Sleep between consecutive sleeps so that ftps is not flooded*/
			usleep(20000);
			/*START:Sleep between consecutive sleeps so that ftps is not flooded*/

			if(sendto(TCPDM1_out_socket, cb.pack[cb.expected_ack%WINDOW].payload, MSS_SIZE, 0,
					(struct sockaddr *)&TCPDM1_out_addr, sizeof(TCPDM1_out_addr))==-1)
			{
				perror("Cannot send data from TCPD_M1 to ftps\n");
				return FAILED;
			}
			printf("sent seq %u %u size data from tcpd_m1 to %s \n",cb.expected_ack,
					(MSS_SIZE),inet_ntoa(TCPDM1_out_addr.sin_addr));
			cbDel();
			cb.expected_ack++;
		}
		/*END: Send all the packets that are in sequence to ftps*/
	}
	/*END: See if the data was retransmitted, if yes then no need to send it to ftps*/
	if(wrap_up_mode==CLOSE_WAIT && cbIsEmpty()==TRUE && cb.expected_ack == fin_seq)
	{

		printf("FIN received and buffer is empty\n");
		/*START: Construct the FIN packet*/
		fin_hdr.th_ack = fin_seq + 1;
		fin_hdr.th_sport = pkt->hdr.th_dport;
		fin_hdr.th_dport = pkt->hdr.th_sport;
		fin_hdr.th_flags = TH_FIN;
		fin_hdr.th_sum = compute_checksum_out(&fin_hdr,&troll_msg.dest);
		memcpy(fin_msg.body, &fin_hdr, sizeof(fin_hdr));
		fin_msg.dest = troll_msg.dest;
		/*END: Construct the FIN packet*/
		printf("Entering last ack\n");
		wrap_up_mode = LAST_ACK;
	}
	print_window();
	return PASSED;
}

/****************************************************
 * Function		: compute_checksum_in
 * Description	: Compute the checksum for the packet received from Troll
 ***************************************************/
u_short compute_checksum_in(struct packet	*pkt, struct sockaddr_in  const * src)
{
	union
	{
		struct
		{
			unsigned int src_addr;
			unsigned int dest_addr;
			unsigned char protocol;
			unsigned char reserved;
			unsigned short int length_tcp;
			struct packet tcp_packet;
		}tcp_pseudohdr;
		unsigned short int shrt_cst[IN_PSEUDO_SIZE];
	}chk_struct;
	int index;
	struct sockaddr_in dest;
	socklen_t addr_len;

	u_short sum = 0;
	addr_len = sizeof(dest);
	bzero((void*)&chk_struct, sizeof(chk_struct));
	memcpy(&chk_struct.tcp_pseudohdr.tcp_packet,(void*)pkt,sizeof(struct packet));

	chk_struct.tcp_pseudohdr.tcp_packet.hdr.th_sum = 0;
	//getsockname(TCPDM1_in_socket, (struct sockaddr *)&dest,&addr_len);
	chk_struct.tcp_pseudohdr.dest_addr = inet_addr(LOCAL_IP);//dest.sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.src_addr = inet_addr(LOCAL_IP); //src->sin_addr.s_addr;
	printf("received address %s\n", inet_ntoa(src->sin_addr));
	chk_struct.tcp_pseudohdr.protocol = 6;
	chk_struct.tcp_pseudohdr.length_tcp = sizeof(struct packet);

	for(index=0;index<IN_PSEUDO_SIZE;index++)
		sum = sum + chk_struct.shrt_cst[index];

	sum = ~sum;
	return sum;
}

/****************************************************
 * Function		: compute_checksum_out
 * Description	: Compute the checksum for the packet sent from M1
 ***************************************************/
u_short compute_checksum_out(struct tcphdr *hdr, struct sockaddr_in const * dest)
{
	union checksum_struct
	{
		struct
		{
			unsigned int src_addr;
			unsigned int dest_addr;
			unsigned char protocol;
			unsigned char reserved;
			unsigned short length_tcp;
			struct tcphdr hdr;
		}tcp_pseudohdr;
		unsigned short shrt_cst[OUT_PSUEDO_SIZE];
	}chk_struct;
	int index;
	//	struct sockaddr_in src;
	socklen_t addr_len;
	u_short sum = 0;

	addr_len = sizeof(struct sockaddr_in);
	bzero((void*)&chk_struct, sizeof(chk_struct));
	memcpy(&chk_struct.tcp_pseudohdr.hdr,(void*)hdr,sizeof(struct tcphdr));
	chk_struct.tcp_pseudohdr.hdr.th_sum = 0;

	chk_struct.tcp_pseudohdr.dest_addr = inet_addr(LOCAL_IP);  			  	//dest->sin_addr.s_addr;
	//getsockname(TCPDM1_in_socket, (struct sockaddr *)&src,&addr_len);
	chk_struct.tcp_pseudohdr.src_addr = inet_addr(LOCAL_IP);  			  	//src.sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.protocol = 6;
	chk_struct.tcp_pseudohdr.length_tcp = sizeof(struct tcphdr);

	for(index=0;index<OUT_PSUEDO_SIZE;index++)
		sum = sum + chk_struct.shrt_cst[index];

	sum = ~sum;
	return sum;
}


int main(int argc, char *argv[])
{
	unsigned int  highest_fd=0;
	u_short chkSum = 0;
	u_short tempSum = 0;
	struct packet *pkt;
	struct timeval rto;
	fd_set TCPDM1_in_set;
	int ret;
	socklen_t addr_len;
	if(argc!=1)
	{
		perror("Format tcpd_m1\n");
		return 1;
	}
	/*START:Establish connection as a server with troll as a client on port 1054*/
	TCPDM1_in_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM1_in_addr.sin_family      = AF_INET;            				// Address family to use
	TCPDM1_in_addr.sin_port        = htons(TCPD_M1_PORT_IN);    // Port number to use
	TCPDM1_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);  				// Listen on any IP address
	if(bind(TCPDM1_in_socket, (struct sockaddr *)&TCPDM1_in_addr, sizeof(TCPDM1_in_addr))!=0)
	{
		perror("Binding failure TCPD_M1\n");
		return 1;
	}
	if(TCPDM1_in_socket>highest_fd)
		highest_fd = TCPDM1_in_socket;
	/*END:Establish connection as a server with troll as a client on port 1054*/

	/*START:Establish connection as a client with ftps on port 1055*/
	TCPDM1_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM1_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM1_out_addr.sin_port        = htons(TCPD_M1_PORT_OUT);     // Port number to use
	TCPDM1_out_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);  			  	// Send to passed IP address
	if(TCPDM1_out_socket>highest_fd)
		highest_fd = TCPDM1_out_socket;
	/*START:Establish connection as a client with ftps on port 1055*/

	/*START:Establish connection as a client with troll on port 1053*/
	troll_send_addr.sin_family      = AF_INET;            			// Address family to use
	troll_send_addr.sin_port        = htons(TROLL_PORT_OUT);     // Port number to use
	/*The IP would be received in the first packet*/
	/*START:Establish connection as a client with troll on port 1052*/

	/*START:Increment the highest FD by 1 for use with select*/
	highest_fd = TCPDM1_in_socket+1;
	/*END:Increment the highest FD by 1 for use with select*/

	/*START: Prepare RTO time wait structure*/
	rto.tv_sec = RTO;
	rto.tv_usec = 0;
	/*END: Prepare RTO time wait structure*/

	addr_len = sizeof(struct sockaddr_in);
	while(1)
	{
		FD_ZERO(&TCPDM1_in_set);
		FD_SET(TCPDM1_in_socket,&TCPDM1_in_set);

		/*START:Listen on all connections*/
		if(wrap_up_mode == LAST_ACK)
		{
			printf("Sending FIN\n");
			if(send_fin()==FAILED)
				return 1;
			ret = select(highest_fd, &TCPDM1_in_set, NULL, NULL, &rto);
		}
		else
			ret = select(highest_fd, &TCPDM1_in_set, NULL, NULL, NULL);
		/*END:Listen on all connections*/

		/*START:If received from troll*/
		if (FD_ISSET(TCPDM1_in_socket, &TCPDM1_in_set))
		{
			/*START: Receive data for the file*/
			bytes_recv_troll = recvfrom(TCPDM1_in_socket, &(troll_msg), sizeof(troll_msg) + 10, 0,
					(struct sockaddr *)&troll_recv_addr, &addr_len);
			/*END: Receive data for the file*/

			/*START: Compute the checksum. Check if it matches the one in the header.*/
			pkt = (struct packet*)troll_msg.body;
			chkSum = compute_checksum_in(pkt,&troll_msg.dest);
			tempSum = pkt->hdr.th_sum;
			/*END: Compute the checksum. Check if it matches the one in the header.*/

			/*START: If checksum is same, send ACK*/
			if(chkSum == tempSum){
				printf("\nThe sent and received checkSum match\n");
				if((pkt->hdr.th_flags&TH_ACK)!=0 && wrap_up_mode == LAST_ACK)
				{
					printf("received the last ack");
					break;
				}
				if(send_ack()==FAILED)
					return FAILED;

			}
			/*END: If checksum is same, send ACK*/

			/*START: If checksum not same drop the packet*/
			else
				printf("\nThe checkSums do not match. Dropping the packet\n");
			/*END: If checksum not same drop the packet*/
		}
		/*END:If received from troll*/
	}
	wrap_up_mode = CLOSED;
	close(TCPDM1_in_socket);
	close(TCPDM1_out_socket);
	return 0;
}
