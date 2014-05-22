/****************************************************
 * Authors: Abhishek and Ananth
 * Date:
 ****************************************************
 * File: TCPD_M2.c
 * Description:
 ***************************************************/
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

/****************************************************
 * Constants declaration
 ***************************************************/
#define PSEUDO_SIZE 516
#define TCP_HEADER_SIZE 20
#define BUFFER_SIZE 64
#define TCPD_M1_PORT_IN 1054
#define TROLL_PORT_OUT 1053
#define FILE_NAME_SIZE 20
#define MSS_SIZE 1000
#define TCPD_M1_PORT_OUT 1055
#define WINDOW 10
#define TRUE 1
#define FALSE 0
#define PASSED 0
#define FAILED 1
#define END_MESSAGE "##$$CONNECTEND$$##"

struct in_addr addr;
struct sockaddr_in   	tcpdm2_addr;
struct sockaddr_in      ftps_addr;
struct from_troll_message	troll_msg;
unsigned int         	TCPDM1_in_socket;   // Server in socket descriptor
unsigned int         	TCPDM1_out_socket;  // Server out socket descriptor
unsigned int 			seq;
struct sockaddr_in   	TCPDM1_in_addr;     // Server1 Internet address
struct sockaddr_in 	 	TCPDM1_out_addr;
struct sockaddr_in   	troll_recv_addr;
struct sockaddr_in   	troll_send_addr;
int                  	addr_len;        		// Internet address length
int                  	bytes_recv_troll;

//Call Format TCPD_M1 <troll-ip> <ftps IP>

/****************************************************
 * Data Structure declaration
 ***************************************************/
/****************************************************
 * struct for sending data to the Troll
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

/****************************************************
 * struct for sending data to the Troll
 ***************************************************/
struct metadata
{
	unsigned int size;
	char name[FILE_NAME_SIZE];
};

/****************************************************
 * struct for sending data to the Troll
 ***************************************************/
struct packet
{
	struct tcphdr hdr;
	char payload[MSS_SIZE];
};

/*struct tcphdr {
	unsigned short source;
	unsigned short dest;
	unsigned long seq;
	unsigned long ack_seq;
#  if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned short res1:4;
	unsigned short doff:4;
	unsigned short fin:1;
	unsigned short syn:1;
	unsigned short rst:1;
	unsigned short psh:1;
	unsigned short ack:1;
	unsigned short urg:1;
	unsigned short res2:2;
#  elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned short doff:4;
	unsigned short res1:4;
	unsigned short res2:2;
	unsigned short urg:1;
	unsigned short ack:1;
	unsigned short psh:1;
	unsigned short rst:1;
	unsigned short syn:1;
	unsigned short fin:1;
#  endif
	unsigned short window;
	unsigned short check;
	unsigned short urg_ptr;
};*/

/****************************************************
 * struct for sending data to the Troll
 ***************************************************/
struct
{
	int start, count;
	struct packet pack[WINDOW];
	unsigned expected_ack;
	unsigned exists [WINDOW];
}cb;

/****************************************************
 * struct for sending data to the Troll
 ***************************************************/
union checksum_struct
{
	struct
	{
		unsigned int src_addr;
		unsigned int dest_addr;
		unsigned char protocol;
		unsigned char reserved;
		unsigned short length_tcp;
		struct packet tcp_packet;
	}tcp_pseudohdr;
	unsigned short shrt_cst[PSEUDO_SIZE];
};

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
void cbInit()
{
	cb.start = 0;
	cb.count = 0;
	cb.expected_ack = 0;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int cbIsFull()
{
	if(cb.count>=WINDOW)
		return TRUE;
	else
		return FALSE;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int cbIsEmpty()
{
	if(cb.count <= 0)
		return TRUE;
	else
		return FALSE;
}

/****************************************************
 * Function: cbInit
 * Description:
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
 * Function: cbInit
 * Description:
 ***************************************************/
int cbWrite(struct packet *p)
{
	if(cbIsFull()==FALSE)
	{
		memcpy(&cb.pack[cb.start + cb.count], p, sizeof(struct packet));
		cb.exists[cb.start + cb.count] = TRUE;
		cb.count = cb.count + 1;
		return PASSED;
	}
	else
		return FAILED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int send_ack()
{
	struct packet			    *pkt;
	struct tcphdr 				hdr;
	struct to_troll_message to_troll;

	/*START: Construct the packet*/
	pkt = (struct packet*)troll_msg.body;
	hdr.th_ack = pkt->hdr.th_seq+1;
	hdr.th_sport = pkt->hdr.th_dport;
	hdr.th_dport = pkt->hdr.th_sport;
	hdr.th_flags = TH_ACK;
	memcpy(to_troll.body, &hdr, sizeof(hdr));
	to_troll.dest = troll_msg.dest;
	/*END: Construct the packet*/

	/*START: Send the acknowledgment to troll*/
	printf("Recvd %u size data to tcpd_m1 from %s %d\n",(bytes_recv_troll),inet_ntoa(addr),pkt->hdr.th_seq);
	if( sendto(TCPDM1_in_socket, &to_troll, sizeof(to_troll), 0,
			(struct sockaddr *)&troll_send_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("Could not send the acknowledgement");
		return FAILED;
	}
	printf("send ACK for seq %d\n",pkt->hdr.th_seq);
	fflush(stdout);
	/*END: Send the acknowledgment to troll*/

	/*START: See if the data was retransmitted, if yes then no need to send it to ftps*/
	if(pkt->hdr.th_seq>=cb.expected_ack)
	{
		/*START: Buffer the packet*/
		cbWrite(pkt);
		/*END: Buffer the packet*/

		/*START: Send all the packets that are in sequence to ftps*/
		while(cb.exists[cb.expected_ack%WINDOW] == TRUE)
		{
			if(sendto(TCPDM1_out_socket, cb.pack[cb.expected_ack%WINDOW].payload, MSS_SIZE, 0,
					(struct sockaddr *)&TCPDM1_out_addr, sizeof(TCPDM1_out_addr))==-1)
			{
				perror("Cannot send data from TCPD_M1 to ftps\n");
				return FAILED;
			}
			cbDel();
			addr.s_addr = ftps_addr.sin_addr.s_addr;
			cb.expected_ack++;
			printf("sent %u size data from tcpd_m1 to %s\n",(MSS_SIZE),inet_ntoa(addr));
		}
		/*END: Send all the packets that are in sequence to ftps*/
	}
	/*END: See if the data was retransmitted, if yes then no need to send it to ftps*/

}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
void compute_checksum(struct packet *p)
{
	/*TCPDM1_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM1_out_addr.sin_port        = htons(TCPD_M1_PORT_IN);    // Port number to use
	TCPDM1_out_addr.sin_addr.s_addr = inet_addr(argv[1]);  					// Send to passed IP address
	 */

	union checksum_struct chk_struct;
	unsigned long sum;
	int i;
	bzero((void*)&chk_struct, PSEUDO_SIZE);
	memcpy(&chk_struct.tcp_pseudohdr.tcp_packet,p,sizeof(struct packet));

	chk_struct.tcp_pseudohdr.dest_addr = inet_addr("127.0.0.1");//tcpdm2_addr.sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.src_addr = inet_addr("127.0.0.1");
	chk_struct.tcp_pseudohdr.protocol = 6;
	chk_struct.tcp_pseudohdr.length_tcp = sizeof(struct packet);

	for(i=0;i<PSEUDO_SIZE;i++)
		sum = sum + chk_struct.shrt_cst[i];
	sum = ~sum;

	p->hdr.th_sum = sum;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int main(int argc, char *argv[])
{

	if(argc!=3)
	{
		perror("Format tcpd_m1 <troll-ip> <FTPS IP>\n");
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
	/*END:Establish connection as a server with troll as a client on port 1054*/

	/*START:receive destination details to send it to this addr*/
	/*bytes_recv_troll = recvfrom(TCPDM1_in_socket, &troll_msg, sizeof(troll_msg), 0,
			(struct sockaddr *)&troll_addr, &addr_len);
	memcpy(&ftps_addr,troll_msg.body,sizeof(ftps_addr));
	addr.s_addr = troll_addr.sin_addr.s_addr;
	printf("Recvd %u size data to tcpd_m1 from %s\n",(bytes_recv_troll),inet_ntoa(addr));*/
	/*END:receive meta data of the file*/

	/*START:Establish connection as a client with ftps on port 1055*/
	TCPDM1_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM1_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM1_out_addr.sin_port        = htons(TCPD_M1_PORT_OUT);     // Port number to use
	TCPDM1_out_addr.sin_addr.s_addr = inet_addr(argv[2]);  			  	// Send to passed IP address
	/*START:Establish connection as a client with ftps on port 1052*/

	/*START:Establish connection as a client with ftps on port 1053*/
	troll_send_addr.sin_family      = AF_INET;            			// Address family to use
	troll_send_addr.sin_port        = htons(TROLL_PORT_OUT);     // Port number to use
	troll_send_addr.sin_addr.s_addr = inet_addr(argv[1]);  			  	// Send to passed IP address
	/*START:Establish connection as a client with ftps on port 1052*/

	addr_len = sizeof(struct sockaddr_in);
	while(1)
	{
		/*START: Receive data for the file*/
		bytes_recv_troll = recvfrom(TCPDM1_in_socket, &(troll_msg), sizeof(troll_msg) + 10, 0,
				(struct sockaddr *)&troll_recv_addr, &addr_len);
		addr.s_addr = troll_recv_addr.sin_addr.s_addr;
		/*END: Receive data for the file*/

		/*START: Send the ACK for the received packet*/
		if(send_ack()==FAILED)
			return FAILED;
		/*END: Send the ACK for the received packet*/

		if (strcmp(troll_msg.body,END_MESSAGE)==0)
			break;
	}
	close(TCPDM1_in_socket);
	close(TCPDM1_out_socket);
	return 0;
}
