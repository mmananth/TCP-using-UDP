/****************************************************
 * Authors: Abhishek and Ananth
 * Date:
 ****************************************************
 * File: TCPD_M2.c
 * Description:
 ***************************************************/
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>

#define BUFFER_SIZE 64
#define TCPD_M2_PORT_IN 1051
#define TCP_HEADER_SIZE 20
#define RANDOM_TIMER_PORT 10052
#define RANDOM_TROLL_PORT 10053
#define FILE_NAME_SIZE 20
#define MSS_SIZE 1000
#define TCPD_M2_PORT_OUT 1053
#define TCPD_M1_PORT_IN 1054
#define TCPD_M2_TIMER_OUT 1052
#define END_MESSAGE "##$$CONNECTEND$$##"
#define PSEUDO_SIZE 516
#define TIMEVAL2UNSIGNED(XX) ((XX.tv_sec*1000) + (XX.tv_usec/1000))
#define WINDOW 10
#define BIT20 512
//Call Format tcpd_m2 <troll-ip> <TCPD_M1 IP> <TIMER IP>
#define SEND_BUFFER 64
#define TRUE 1
#define FALSE 0
#define PASSED 0
#define FAILED 1

/****************************************************
 * Constants declaration
 ***************************************************/
/* For the RTO calculation
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

float Err, A = 0;									/*Initial value of A has to be 0*/
unsigned long int M = 0;
float D = 3;										/*Initial value has to be 3*/
unsigned int RTO = 6000;								/*RTO is initially set to 6*/
static int rttCount = 0;
struct timeval received_time[SEND_BUFFER];

struct metadata			in_meta;
unsigned int         	TCPDM2_in_socket;   		/* Server in socket descriptor */
struct to_troll_message troll_msg;
struct sockaddr_in   	TCPDM2_in_addr;     		/* Server1 Internet address */
unsigned int         	TCPDM2_out_socket;  		/* Server out socket descriptor */
struct sockaddr_in 	 	TCPDM2_out_addr;
unsigned int         	TCPDM2_timer_socket;  		/* Server out socket descriptor */
struct sockaddr_in   	TCPDM2_timer_addr;     		/* Server1 Internet address */
unsigned int 			highest_fd;
struct sockaddr_in   	ftpc_addr;
int                  	addr_len;        			/* Internet address length */
int                  	bytes_sent_troll;
fd_set 					TCPDM2_in_set;



/*
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
};*/

/****************************************************
 * Data Structure declaration
 ***************************************************/
/****************************************************
 * Modes for the timer
 ***************************************************/
enum timer_modes{
	INSERT_MODE=0,
	DELETE_MODE,
}timer_modes;

/****************************************************
 * struct for sending data to the Troll
 ***************************************************/
struct to_troll_message
{
	struct sockaddr_in dest; //Address of the destination
	char body[MSS_SIZE + TCP_HEADER_SIZE];
};

/****************************************************
 * struct for data received from the Troll
 ***************************************************/
struct from_troll_message
{
	struct sockaddr_in source; //Address of the source
	char body[TCP_HEADER_SIZE];
};

/****************************************************
 *
 ***************************************************/
struct metadata
{
	unsigned int size;
	char name[FILE_NAME_SIZE];
};

/****************************************************
 * Packet structure for the data sent from ftpc to ftps
 ***************************************************/
struct packet
{
	struct tcphdr hdr;
	char payload[MSS_SIZE];
};

/****************************************************
 * struct for the circular buffer
 ***************************************************/
struct
{
	int start, count;
	struct packet pack[SEND_BUFFER];
	struct timeval sent_time[SEND_BUFFER];
	struct timeval received_time[SEND_BUFFER];
	unsigned ack_map:WINDOW;
	unsigned sent;
}cb;

/****************************************************
 * struct for data sent to timer
 ***************************************************/
struct timer_send_struct
{
	unsigned int seq_no;
	struct timeval AERT;
	int mode;
};

/****************************************************
 * struct for calculating the TCP checksum
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
	cb.ack_map = 0x00000;
	cb.sent = 0;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int cbIsFull()
{
	if(cb.count>=SEND_BUFFER)
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
	if(cb.count == 0)
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
	cb.count--;
	cb.start = (cb.start + 1)%SEND_BUFFER;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int cbWrite(struct packet p)
{
	if(cbIsFull()==FALSE){
		memcpy(&cb.pack[cb.start + cb.count], &p, sizeof(struct packet));
		cb.count = cb.count + 1;
		/*if(cb.count == SEND_BUFFER)
		{
			cb.start = (cb.start + 1) % SEND_BUFFER;
			cb.count = 0;
		}
		else
		{
		cb.count = cb.count + 1;
		cb.start = (cb.start + 1)%SEND_BUFFER;
		}*/
		return PASSED;
	}
	else
		return FAILED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
void compute_checksum(struct packet *p)
{
	union checksum_struct chk_struct;
	unsigned long sum;
	int i;
	bzero((void*)&chk_struct, PSEUDO_SIZE);
	memcpy(&chk_struct.tcp_pseudohdr.tcp_packet,p,sizeof(struct packet));

	chk_struct.tcp_pseudohdr.dest_addr = inet_addr("127.0.0.1");
	chk_struct.tcp_pseudohdr.src_addr = inet_addr("127.0.0.1");//TCPDM2_out_addr.sin_addr.s_addr;
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
/* Start the timer before sending a packet */
int start_Time(int index)
{
	printf("Starting timer\n");
	gettimeofday(&cb.sent_time[index],NULL);
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
/* Stop the timer after receiving an ACK */
int end_Time(int index)
{
	printf("Stopping timer\n");
	gettimeofday(&cb.received_time[index],NULL);
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
/* Calculate the RTT */
int calculate_RTT(int index)
{
	printf("calculate_RTT - index is :%d\n", index);
	end_Time(index);
	/* calculate mean RTT and RTO*/
	/*if(cb.sent_time[index].tv_sec == cb.received_time[index].tv_sec)
	{
		rttCount++;
		M = cb.received_time[index].tv_usec-cb.sent_time[index].tv_usec;
		printf("cb.received_time[index].tv_usec: %ld\n", cb.received_time[index].tv_usec);
		printf("cb.sent_time[index].tv_usec: %ld\n", cb.sent_time[index].tv_usec);
		printf("1. The RTT value is: %lu\n", M);
	}
	else
	{*/
	rttCount++;
	/*long int sec = cb.received_time[index].tv_sec - cb.sent_time[index].tv_sec;
		long int usec = cb.received_time[index].tv_usec - cb.sent_time[index].tv_usec;*/
	M = (TIMEVAL2UNSIGNED(cb.received_time[index])) -
			(TIMEVAL2UNSIGNED(cb.sent_time[index]));//sec + usec;
	printf("cb.received_time[index].tv_usec: %ld\n", cb.received_time[index].tv_usec);
	printf("cb.sent_time[index].tv_usec: %ld\n", cb.sent_time[index].tv_usec);
	/*printf("The sec value is: %ld\n", sec);
	printf("The usec value is: %ld\n", usec);*/
	printf("2. The RTT value is: %lu\n", M);
	//}
	printf("Mean RTT is: %lu\n", M);
	return M;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
/*This is only for the initial SYN packet*/
int calculateInitialRTO(int index)
{
	/*For the very first packet. RTO = 6*/
	/*if(index == 0){
		RTO = A + 2*D;
	}*/
	/*For first timeout. RTO = 12*/
	/*else if(index == 1){
		RTO = A + 4*D;
	}*/
	/*For second timeout. RTO = 24*/
	/*else if(index == 2){
		RTO = A + 8*D;
	}*/
	/*For third and subsequent timeouts, RTO = 48*/
	/*else{
		RTO = A + 16*D;
	}*/
	RTO = 6000;
	printf("The initial RTO is: %d\n", RTO);
	return RTO;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
/*This is for packets after the initial SYN. Only segments containing data are ACKed*/
void calculate_RTO(unsigned long int M)
{
	printf("The value of M is: %lu\n", M);
	Err = M - A;
	A = A + g*Err;
	D = D + h * (abs(Err) - D);
	RTO = A + 4*D + 1000;
	printf("The RTO is: %u\n", RTO);
	//return RTO;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int send_data_troll(unsigned index)
{
	struct timeval curr_time;
	struct timer_send_struct timer_data;
	struct in_addr addr;
	/*START:Calculate the Expected Return time for the packet and send to timer*/
	start_Time(index);

	/* Listen for the ACK and then stop the timer. Calculate the RTT from the start and end timings */
	/*if(index == 0)
	{
		printf("Calling calculateInitialRTO with index :%d\n", index);
		RTO = calculateInitialRTO(index);
	}
	else
	{
		printf("Calling calculate_RTO with index :%d and M: %lu\n", index, M);
		RTO = calculate_RTO(M);
		printf("The RTO value is: %u\n", RTO);
	}*/

	timer_data.AERT.tv_sec =  cb.sent_time[index].tv_sec + RTO/1000;
	timer_data.AERT.tv_usec =  cb.sent_time[index].tv_usec + (RTO%1000);
	timer_data.mode = INSERT_MODE;
	timer_data.seq_no = cb.pack[index].hdr.th_seq;
	if(sendto(TCPDM2_timer_socket, (void*)&timer_data, sizeof(struct timer_send_struct), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, sizeof(TCPDM2_timer_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TIMER\n");
		return FAILED;
	}
	printf("Sent %u sequence data from tcpd_m2 to timer sec %lu usec %lu timesec %lu usec %lu\n",
			timer_data.seq_no,timer_data.AERT.tv_sec,timer_data.AERT.tv_usec,
			cb.sent_time[index].tv_sec, cb.sent_time[index].tv_usec );
	/*END:Calculate the Expected Return time for the packet*/

	/*START:Send data to troll*/
	memcpy(troll_msg.body,&cb.pack[index],sizeof(struct packet));
	if(sendto(TCPDM2_out_socket, (void*)&troll_msg, sizeof(struct to_troll_message), 0,
			(struct sockaddr *)&TCPDM2_out_addr, sizeof(TCPDM2_out_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TROLL\n");
		return FAILED;
	}
	addr.s_addr = TCPDM2_out_addr.sin_addr.s_addr;
	printf("Sent %u sequence data from tcpd_m2 troll %s %u\n",timer_data.seq_no,inet_ntoa(addr), ntohs(TCPDM2_out_addr.sin_port));

	/*END:Send data to troll*/
	return PASSED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int window_change()
{
	/*START: The send window might not be full, send data in that case*/
	if((cb.start + WINDOW) > cb.sent)
	{
		//printf("Condition (%d + %d) > %d satisfied\n",cb.start,WINDOW, cb.sent);
		send_data_troll(cb.sent);
		cb.sent++;
	}
	/*END: The send window might not be full, send data in that case*/

	/*START: Probably some ACKs were received, check if first packet in window ACKed*/
	while((cb.ack_map&BIT20)!=0)
	{
		cb.ack_map = cb.ack_map<<1;
		cbDel();
		/*START:As ACKs were received, send new packets modify the window*/
		if (cb.count > (cb.sent - cb.start))
		{
			if(send_data_troll(cb.sent)==FAILED)
				return FAILED;
			cb.sent++;
			//addr.s_addr = TCPDM2_out_addr.sin_addr.s_addr;
			//printf("Sent %u size data from tcpd_m2 to %s\n",sizeof(troll_msg),inet_ntoa(addr));
		}
		/*START:As ACKs were received, send new packets modify the window*/
	}
	/*END:Probably some ACKs were received, check if first packet in window ACKed*/
	return PASSED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int process_ftpc_data()
{

	int bytes_recv_ftpc;
	struct in_addr addr;
	struct packet p;
	if(!cbIsFull())
	{
		bzero((void*)&p, sizeof(p));
		/*START: Receive data for the file and add it to buffer*/
		bytes_recv_ftpc = recvfrom(TCPDM2_in_socket, p.payload, MSS_SIZE, 0,
				(struct sockaddr *)&ftpc_addr, &addr_len);
		addr.s_addr = ftpc_addr.sin_addr.s_addr;
		printf("Received %u size data to tcpd_m2 from %s\n",bytes_recv_ftpc,inet_ntoa(addr));

		/*START:Packetize the data before adding it to the buffer*/
		p.hdr.th_sport = TCPD_M1_PORT_IN;
		p.hdr.th_dport = TCPD_M2_PORT_OUT;
		p.hdr.th_seq = (cb.start+cb.count);
		compute_checksum(&p);
		cbWrite(p);
		/*END:Packetize the data before adding it to the buffer*/
		/*END: Receive data for the file and add it to buffer*/
		window_change();
	}
	return PASSED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int process_timer_data()
{
	int bytes_recv_timer;
	unsigned int seq_no;
	/*START: Receive Timeouts for re transmission*/
	addr_len = sizeof(struct sockaddr_in);
	bytes_recv_timer = recvfrom(TCPDM2_timer_socket, &seq_no, sizeof(unsigned int), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, &addr_len);
	printf("Received %u size data to tcpd_m2 from timer %d timed out\n",bytes_recv_timer,seq_no);
	/*END: Receive ACKs*/
	send_data_troll(seq_no);
	return PASSED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int process_troll_data()
{
	struct from_troll_message troll_msg;
	struct timer_send_struct timer_data;
	struct tcphdr *hdr;
	unsigned seq_number;
	int bytes_recv_troll;

	/*START: Receive ACKs*/
	addr_len = sizeof(struct sockaddr_in);
	bytes_recv_troll = recvfrom(TCPDM2_out_socket, &troll_msg, sizeof(struct from_troll_message), 0,
			(struct sockaddr *)&TCPDM2_out_addr, &addr_len);

	/*END: Receive ACKs*/

	/*START: Discard ACK if not in the window*/
	hdr = (struct tcphdr *)troll_msg.body;
	seq_number = hdr->th_ack - 1;
	printf("Received %u size data seq %u acked to tcpd_m2 from troll\n",bytes_recv_troll,seq_number);
	if(seq_number<cb.start||seq_number>(cb.sent-1))
		return PASSED;
	/*END: Discard ACK if not in the window*/

	printf("Calling calculate_RTT with seq_number: %d\n", seq_number);
	M = calculate_RTT(seq_number);	//for now
	printf("process_troll_data - The value of M is: %lu\n", M);


	/*START:Delete the entry from the timer*/
	timer_data.mode = DELETE_MODE;
	timer_data.seq_no = seq_number;
	if(sendto(TCPDM2_timer_socket, (void*)&timer_data, sizeof(struct timer_send_struct), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, sizeof(TCPDM2_timer_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TIMER\n");
		return FAILED;
	}
	/*END:Delete the entry from the timer*/

	/*START: Recalculate the RTO*/
	calculate_RTO(M);
	/*END: Recalculate the RTO*/

	/*START:Modify the window*/
	cb.ack_map = cb.ack_map|(1<<((WINDOW - (seq_number - cb.start))) - 1);
	//printf("ack map after ack rceived %u",cb.ack_map);
	window_change();
	/*END:Modify the window*/
	return PASSED;
}

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int main(int argc, char *argv[])
{
	int ret = 0;

	if(argc!=4)
	{
		perror("Format tcpd_m2 <troll-ip> <TCPD_M1 IP> <TIMER IP>\n");
		return 1;
	}

	//RTO = 3000;
	cbInit();

	/*START:Establish connection as a client with TROLL on port 1053*/
	TCPDM2_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
	/*TCPDM2_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_out_addr.sin_port        = htons(RANDOM_TROLL_PORT);    // Port number to use
	TCPDM2_out_addr.sin_addr.s_addr = htonl(INADDR_ANY);  					// Send to passed IP address
	if(bind(TCPDM2_out_socket, (struct sockaddr *)&TCPDM2_out_addr, sizeof(TCPDM2_out_addr))!=0)
	{
		perror("Binding failure tcpd_m2\n");
		return 1;
	}*/
	TCPDM2_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_out_addr.sin_port        = htons(TCPD_M2_PORT_OUT);    // Port number to use
	TCPDM2_out_addr.sin_addr.s_addr = inet_addr(argv[1]);  					// Send to passed IP address

	/*END:Establish connection as a client with TROLL on port 1053*/

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
	/*END:Establish connection as a server with ftpc as a client on port 1051*/

	/*START:receive destination details and configure for troll to send it to this addr*/
	troll_msg.dest.sin_family      = AF_INET;            			// Address family to use
	troll_msg.dest.sin_port        = htons(TCPD_M1_PORT_IN);  // Port number to use
	troll_msg.dest.sin_addr.s_addr = inet_addr(argv[2]);  		// Send to passed IP addres
	/*END:receive destination details and configure for troll to send it to this addr*/

	/*START:Establish connection as a client with timer on port 1052*/
	TCPDM2_timer_socket = socket(AF_INET, SOCK_DGRAM, 0);
	/*TCPDM2_timer_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_timer_addr.sin_port        = htons(RANDOM_TIMER_PORT);    // Port number to use
	TCPDM2_timer_addr.sin_addr.s_addr = htonl(INADDR_ANY);  				// Send to passed IP address
	if(bind(TCPDM2_timer_socket, (struct sockaddr *)&TCPDM2_timer_addr, sizeof(TCPDM2_timer_addr))!=0)
	{
		perror("Binding failure tcpd_m2\n");
		return 1;
	}*/
	TCPDM2_timer_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_timer_addr.sin_port        = htons(TCPD_M2_TIMER_OUT);    // Port number to use
	TCPDM2_timer_addr.sin_addr.s_addr = inet_addr(argv[3]);  					// Send to passed IP address

	/*END:Establish connection as a client with timer on port 1052*/

	/*START:Get the highest descriptor value for the select to function*/
	highest_fd = TCPDM2_in_socket>TCPDM2_out_socket?TCPDM2_in_socket:TCPDM2_out_socket;
	highest_fd = highest_fd>TCPDM2_timer_socket?highest_fd:TCPDM2_timer_socket;
	highest_fd++;
	/*printf("TCPDM2_in_socket - %d", TCPDM2_in_socket);
	printf("TCPDM2_out_socket - %d", TCPDM2_out_socket);
	printf("TCPDM2_timer_socket - %d", TCPDM2_timer_socket);
	printf("highest_fd - %d", highest_fd);*/
	/*END:Get the highest descriptor value for the select to function*/

	while(1)
	{
		FD_ZERO(&TCPDM2_in_set);
		FD_SET(TCPDM2_timer_socket,&TCPDM2_in_set);
		FD_SET(TCPDM2_in_socket,&TCPDM2_in_set);
		FD_SET(TCPDM2_out_socket,&TCPDM2_in_set);

		/*START:Listen on all connections*/
		ret = select(highest_fd, &TCPDM2_in_set, NULL, NULL, NULL);
		/*END:Listen on all connections*/

		/*START: If received from timer*/
		if (FD_ISSET(TCPDM2_timer_socket, &TCPDM2_in_set))
		{
			if (process_timer_data()<0)
				return 1;

		}
		/*END: If received from ftpc*/

		/*START: If received from troll*/
		if (FD_ISSET(TCPDM2_out_socket, &TCPDM2_in_set))
		{
			if (process_troll_data()<0)
				return 1;
		}
		/*END: If received from ftpc*/

		/*START: If received from ftpc*/
		if (FD_ISSET(TCPDM2_in_socket, &TCPDM2_in_set))
		{
			if (process_ftpc_data()<0)
				return 1;
		}
		/*END: If received from ftpc*/
	}
	close(TCPDM2_in_socket);
	close(TCPDM2_out_socket);
	return 0;
}
