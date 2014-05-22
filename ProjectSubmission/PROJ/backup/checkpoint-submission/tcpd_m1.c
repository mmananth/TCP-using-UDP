#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define PSEUDO_SIZE 516
#define TCP_HEADER_SIZE 20
#define BUFFER_SIZE 64
#define TCPD_M1_PORT_IN 1054
#define TROLL_PORT_OUT 1053
#define FILE_NAME_SIZE 20
#define MSS_SIZE 1000
#define TCPD_M1_PORT_OUT 1055
#define END_MESSAGE "##$$CONNECTEND$$##"

//Call Format TCPD_M1 <troll-ip> <ftps IP>
struct in_addr addr;

struct sockaddr_in   	tcpdm2_addr;
struct sockaddr_in      ftps_addr;

struct from_troll_message
{
	struct sockaddr_in dest; //Address of the destination
	char body[MSS_SIZE + TCP_HEADER_SIZE];
};

struct to_troll_message
{
	struct sockaddr_in dest; //Address of the destination
	char body[TCP_HEADER_SIZE];
};

struct metadata
{
	unsigned int size;
	char name[FILE_NAME_SIZE];
};

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


int main(int argc, char *argv[])
{
	struct from_troll_message	troll_msg;
	struct to_troll_message   to_Troll_msg;
	unsigned int         	TCPDM1_in_socket;   // Server in socket descriptor
	unsigned int         	TCPDM1_out_socket;  // Server out socket descriptor
	unsigned int         	TCPDM1_troll_out_socket;  // Server out socket descriptor
	struct sockaddr_in   	TCPDM1_in_addr;     // Server1 Internet address
	struct sockaddr_in 	 	TCPDM1_out_addr;
	struct sockaddr_in   	troll_recv_addr;
	struct sockaddr_in   	troll_send_addr;
	int                  	addr_len;        		// Internet address length
	int                  	bytes_recv_troll;
	unsigned int seq;
	struct packet			    *pkt;

	struct tcphdr 				hdr;
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
	//TCPDM1_troll_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
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

		/* Send the ACK for the received packet */

		pkt = (struct packet*)troll_msg.body;
		hdr.th_ack = pkt->hdr.th_seq+1;
		hdr.th_sport = pkt->hdr.th_dport;
		hdr.th_dport = pkt->hdr.th_sport;
		hdr.th_flags = TH_ACK;
		memcpy(to_Troll_msg.body, &hdr, sizeof(hdr));
		to_Troll_msg.dest = troll_msg.dest;
		printf("Recvd %u size data to tcpd_m1 from %s %d\n",(bytes_recv_troll),inet_ntoa(addr),pkt->hdr.th_seq);
		if( sendto(TCPDM1_in_socket, &to_Troll_msg, sizeof(to_Troll_msg), 0,
				(struct sockaddr *)&troll_send_addr, sizeof(struct sockaddr_in)) < 0)
		{
			perror("Could not send the acknowledgement");
			return 1;
		}
		printf("send ACK for seq %d\n",pkt->hdr.th_seq);
		fflush(stdout);
		/*START: Send it to ftps*/

		if(sendto(TCPDM1_out_socket, pkt->payload, MSS_SIZE, 0,
				(struct sockaddr *)&TCPDM1_out_addr, sizeof(TCPDM1_out_addr))==-1)
		{
			perror("Cannot send data from TCPD_M1 to ftps\n");
			return 1;
		}
		addr.s_addr = ftps_addr.sin_addr.s_addr;
		printf("sent %u size data from tcpd_m1 to %s\n",(MSS_SIZE),inet_ntoa(addr));

		/*END: Send it to ftps*/

		if (strcmp(troll_msg.body,END_MESSAGE)==0)
			break;
	}
	close(TCPDM1_in_socket);
	close(TCPDM1_out_socket);
	return 0;

}
