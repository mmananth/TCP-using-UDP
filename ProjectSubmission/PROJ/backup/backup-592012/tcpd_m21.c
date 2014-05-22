#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#define BUFFER_SIZE 64
#define TCPD_M2_PORT_IN 1051
#define FILE_NAME_SIZE 20
#define MSS_SIZE 1000
#define TCPD_M2_PORT_OUT 1053
#define TCPD_M1_PORT_IN 1054
#define TCPD_M2_TIMER_OUT 1052
#define END_MESSAGE "##$$CONNECTEND$$##"
#define TIMEVAL2UNSIGNED(XX) ((XX.tv_sec*1000) + (XX.tv_usec/1000))
#define WINDOW 20
#define PSEUDO_SIZE 516
#define BIT20 0x80000
//Call Format tcpd_m2 <troll-ip> <TCPD_M1 IP> <TIMER IP>
#define SEND_BUFFER 64
#define TRUE 1
#define FALSE 0
#define PASSED 0
#define FAILED 1

enum timer_modes{
	INSERT_MODE=0,
	DELETE_MODE,
}timer_modes;
struct tcphdr {
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
};

struct
{
	unsigned ack_map:WINDOW;
	unsigned sent;
	unsigned ack;
}sl_window;

struct metadata
{
	unsigned int size;
	char name[FILE_NAME_SIZE];
} ;
struct troll_message
{
	struct sockaddr_in dest; //Address of the destination
	char body[MSS_SIZE];
};

struct packet
{
	struct tcphdr hdr;
	char payload[MSS_SIZE];
};
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

void checkSum(packet *p)
{
	TCP_PSEUDOHEADER *tcp_psHdr;
	union checksum_struct chk_struct;
	unsigned long sum;
	int valRem = 0;
	int index = 0;
	bzero((void*)chk_struct, PSEUDO_SIZE);

	chk_struct.tcp_pseudohdr.dest_addr = inet_addr("127.0.0.1");
	chk_struct.tcp_pseudohdr.src_addr = TCPDM2_out_addr.sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.protocol = 6;


	/* Calculate the checksum for the TCP segment */
	valRem = sizeof(*p);
	while(valRem > 1)
	{
		sum += *p++;
		valRem -= 2;
	}

	/* Check if valRem has one byte left. If it has, pad it to get a 16 bit word */
	if(valRem > 0)
	{
		sum += *p & ntohs(0xFF00);
	}

	/* Calculate the sum of the TCP pseudo header */
		sum += tcp_psHdr->src_addr[0];
	sum += tcp_psHdr->src_addr[1];
	sum += tcp_psHdr->dest_addr[0];
	sum += tcp_psHdr->dest_addr[1];
	sum += htons(tcp_psHdr->length_tcp);
	sum += htons(tcp_psHdr->protocol_tcp);


	/* keep only the last 16 bits of the sum */
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	/* Take the one's complement of the sum */
	sum = ~sum;

	p->tcphdr_hdr.check = 0;
	p->tcphdr_hdr.check = sum;

	/*p->tcphdr_hdr.check = p->tcphdr_hdr.source + p->tcphdr_hdr.dest + p->tcphdr_hdr.seq + p->tcphdr_hdr.ack_seq + p->tcphdr_hdr.res1 + p->tcphdr_hdr.urg +
				p->tcphdr_hdr.ack + p->tcphdr_hdr.psh + p->tcphdr_hdr.rst + p->tcphdr_hdr.syn + p->tcphdr_hdr.fin + p->tcphdr_hdr.window + p->tcphdr_hdr.urg_ptr;*/
}


struct metadata				in_meta;
struct troll_message	troll_msg;
unsigned int         	TCPDM2_in_socket;   // Server in socket descriptor
struct sockaddr_in   	TCPDM2_in_addr;     // Server1 Internet address
unsigned int         	TCPDM2_out_socket;  // Server out socket descritor
struct sockaddr_in 	 	TCPDM2_out_addr;
unsigned int         	TCPDM2_timer_socket;  // Server out socket descriptor
struct sockaddr_in   	TCPDM2_timer_addr;     // Server1 Internet address
unsigned int 					highest_fd;
struct sockaddr_in   	ftpc_addr;
int                  	addr_len;        		// Internet address length
int                  	bytes_sent_troll;
fd_set 								TCPDM2_in_set;
unsigned int 					RTO;

struct cb
{
	int start, count;
	struct packet pack[SEND_BUFFER];
}cb;

struct timer_send_struct
{
	unsigned int seq_no;
	unsigned int AERT;
	int mode;
};

void cbInit()
{
	cb.start = 0;
	cb.count = 0;
}

int cbIsFull()
{
	if(cb.count>=SEND_BUFFER)
		return TRUE;
	else
		return FALSE;
}


int cbIsEmpty()
{
	if(cb.count == 0)
		return TRUE;
	else
		return FALSE;
}
int cbDel()
{
	cb.count--;
	cb.start = (cb.start + 1)%SEND_BUFFER;
}
int cbWrite(struct packet p)
{
	if(cbIsFull()==FALSE){
		memcpy(&cb.pack[cb.start], &p, sizeof(struct packet));
		if(cb.count == SEND_BUFFER)
		{
			cb.start = (cb.start + 1) % SEND_BUFFER;
			cb.count = 0;
		}
		else
		{
			cb.count = cb.count + 1;
			cb.start = (cb.start + 1)%SEND_BUFFER;
		}
	}
}
int send_data_troll()
{
	struct timeval curr_time;
	unsigned int AERT;
	struct timer_send_struct timer_data;

	/*START:Calculate the Expected Return time for the packet and send to timer*/
	gettimeofday(&curr_time,NULL);
	AERT = TIMEVAL2UNSIGNED(curr_time) + RTO;
	timer_data.AERT = INSERT_MODE;
	timer_data.seq_no = sl_window.sent;
	if(sendto(TCPDM2_timer_socket, (void*)&timer_data, sizeof(struct timer_send_struct), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, sizeof(TCPDM2_timer_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TIMER\n");
		return FAILED;
	}
	/*END:Calculate the Expected Return time for the packet*/

	/*START:Send data to troll*/
	if(sendto(TCPDM2_out_socket, (void*)&cb.pack[sl_window.sent], sizeof(struct packet), 0,
			(struct sockaddr *)&TCPDM2_out_addr, sizeof(TCPDM2_out_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TROLL\n");
		return FAILED;
	}
	/*END:Send data to troll*/
	return PASSED;
}
int process_ftpc_data()
{

	int bytes_recv_ftpc;
	struct in_addr addr;
	struct packet p;

	/*START: Receive data for the file and add it to buffer*/
	bytes_recv_ftpc = recvfrom(TCPDM2_in_socket, p.payload, MSS_SIZE, 0,
			(struct sockaddr *)&ftpc_addr, &addr_len);
	addr.s_addr = ftpc_addr.sin_addr.s_addr;
	printf("Received %u size data to tcpd_m2 from %s\n",bytes_recv_ftpc,inet_ntoa(addr));

	/*START:Packetize the data before adding it to the buffer*/
	p.hdr.dest = TCPD_M1_PORT_IN;
	p.hdr.source = TCPD_M2_PORT_OUT;
	p.hdr.seq = cb.start;
	p.hdr.check = 0;
	compute_checksum(&p);
	cbWrite(p);
	/*END:Packetize the data before adding it to the buffer*/
	/*END: Receive data for the file and add it to buffer*/
}

int window_change()
{
	/*START:Probably some ACKs were received, check if first packet in window ACKed*/
	while(sl_window.ack_map&BIT20==1)
	{
		sl_window.ack_map = sl_window.ack_map<<1;
		cbDel();
		/*START:As ACKs were received, send new packets modify the window*/
		if (cbIsEmpty()==FALSE)
		{
			send_data_troll();
			sl_window.sent++;
			//addr.s_addr = TCPDM2_out_addr.sin_addr.s_addr;
			//printf("Sent %u size data from tcpd_m2 to %s\n",sizeof(troll_msg),inet_ntoa(addr));
		}
		/*START:As ACKs were received, send new packets modify the window*/
	}
	/*END:Probably some ACKs were received, check if first packet in window ACKed*/
}

int process_timer_data()
{
	/*START: Receive Timeouts for re transmission*/
	addr_len = sizeof(struct sockaddr_in);
	bytes_recv_ftpc = recvfrom(TCPDM2_out_socket, &p, sizeof(p), 0,
			(struct sockaddr *)&TCPDM2_out_addr, &addr_len);
	addr.s_addr = ftpc_addr.sin_addr.s_addr;
	printf("Received %u size data to tcpd_m2 from %s\n",bytes_recv_ftpc,inet_ntoa(addr));
	/*END: Receive ACKs*/

	return 0;
}
int process_troll_data()
{
	struct packet p;
	struct timer_send_struct timer_data;
	unsigned seq_number;

	/*START: Receive ACKs*/
	addr_len = sizeof(struct sockaddr_in);
	bytes_recv_ftpc = recvfrom(TCPDM2_out_socket, &p, sizeof(p), 0,
			(struct sockaddr *)&TCPDM2_out_addr, &addr_len);
	addr.s_addr = ftpc_addr.sin_addr.s_addr;
	printf("Received %u size data to tcpd_m2 from %s\n",bytes_recv_ftpc,inet_ntoa(addr));
	/*END: Receive ACKs*/

	/*START: Discard ACK if not in the window*/
	seq_number = p.hdr.seq;
	if(p.hdr.seq<cb.start||p.hdr.seq>(sl_window.sent-1))
		return PASSED;
	/*END: Discard ACK if not in the window*/

	/*START: Recalculate the RTO*/
	calculate_RTO();
	/*END: Recalculate the RTO*/

	/*START:Modify the window*/
	sl_window.ack_map = sl_window.ack_map&(1<<(WINDOW - (p.hdr.seq - cb.start)));
	window_change();
	/*END:Modify the window*/

	/*START:Calculate the Expected Return time for the packet and send to timer*/
	timer_data.AERT = DELETE_MODE;
	timer_data.seq_no = p.hdr.seq - cb.start;
	if(sendto(TCPDM2_timer_socket, (void*)&timer_data, sizeof(struct timer_send_struct), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, sizeof(TCPDM2_timer_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TIMER\n");
		return FAILED;
	}
	/*END:Calculate the Expected Return time for the packet*/

	return PASSED;
}




int main(int argc, char *argv[])
{
	int ret = 0;
	if(argc!=4)
	{
		perror("Format tcpd_m2 <troll-ip> <TCPD_M1 IP> <TIMER IP>\n");
		return 1;
	}
	FD_ZERO(&TCPDM2_in_set);


	/*START:Establish connection as a server with ftpc as a client on port 1051*/
	TCPDM2_in_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM2_in_addr.sin_family      = AF_INET;            				// Address family to use
	TCPDM2_in_addr.sin_port        = htons(TCPD_M2_PORT_IN);    // Port number to use
	TCPDM2_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);  				// Listen on any IP address
	if(bind(TCPDM2_in_socket, (struct sockaddr *)&TCPDM2_in_addr, sizeof(TCPDM2_in_addr))!=0)
	{
		perror("Binding failure tcpd_m2\n");
		return 1;
	}
	FD_SET(TCPDM2_in_socket,&TCPDM2_in_set);
	/*END:Establish connection as a server with ftpc as a client on port 1051*/

	/*START:Establish connection as a client with TROLL on port 1053*/
	TCPDM2_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM2_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_out_addr.sin_port        = htons(TCPD_M2_PORT_OUT);    // Port number to use
	TCPDM2_out_addr.sin_addr.s_addr = inet_addr(argv[1]);  					// Send to passed IP address
	FD_SET(TCPDM2_out_socket,&TCPDM2_in_set);
	/*END:Establish connection as a client with TROLL on port 1053*/

	/*START:Establish connection as a client with timer on port 1052*/
	TCPDM2_timer_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM2_timer_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_timer_addr.sin_port        = htons(TCPD_M2_PORT_OUT);    // Port number to use
	TCPDM2_timer_addr.sin_addr.s_addr = inet_addr(argv[3]);  					// Send to passed IP address
	FD_SET(TCPDM2_timer_socket,&TCPDM2_in_set);
	/*END:Establish connection as a client with timer on port 1052*/

	/*START:receive destination details and configure for troll to send it to this addr*/
	troll_msg.dest.sin_family      = AF_INET;            			// Address family to use
	troll_msg.dest.sin_port        = htons(TCPD_M1_PORT_IN);  // Port number to use
	troll_msg.dest.sin_addr.s_addr = inet_addr(argv[2]);  		// Send to passed IP addres
	/*END:receive destination details and configure for troll to send it to this addr*/

	/*START:Get the highest descriptor value for the select to function*/
	highest_fd = TCPDM2_in_socket>TCPDM2_out_socket?TCPDM2_in_socket:TCPDM2_out_socket;
	highest_fd = highest_fd>TCPDM2_timer_socket?highest_fd:TCPDM2_timer_socket;
	highest_fd++;
	/*END:Get the highest descriptor value for the select to function*/

	while(1)
	{

		/*START:Listen on all connections*/
		ret = select(highest_fd, &TCPDM2_in_set, NULL, NULL, NULL);
		/*END:Listen on all connections*/

		/*START: If received from ftpc*/
		if (FD_ISSET(TCPDM2_in_socket, &TCPDM2_in_set))
			if (process_ftpc_data()<0)
				return 1;
		/*END: If received from ftpc*/

		/*START: If received from ftpc*/
		if (FD_ISSET(TCPDM2_in_socket, &TCPDM2_in_set))
			if (process_timer_data()<0)
				return 1;
		/*END: If received from ftpc*/

		/*START: If received from ftpc*/
		if (FD_ISSET(TCPDM2_in_socket, &TCPDM2_in_set))
			if (process_troll_data()<0)
				return 1;
		/*END: If received from ftpc*/


		if (strcmp(troll_msg.body,END_MESSAGE)==0)
			break;
	}
	close(TCPDM2_in_socket);
	close(TCPDM2_out_socket);
	return 0;

}
