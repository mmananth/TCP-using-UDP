/****************************************************************************************************************************************
 * Authors	: Abhishek and Ananth
 * Date		: June-6-2012
 ****************************************************************************************************************************************
 * File				: TCPD_M2.c
 * Description		: This is the Daemon process on the client side. It has several functions and mimics the functionality of TCP by ensuring
 * 					  that packets sent were received by receiver. Some of its functions are:
 * 					  1. Receive data packets from the client and send it to TCPD_M1 via Troll.
 * 					  2. Calculate RTO for a packet and send that value to a timer
 * 					  3. Slide a window forward when an ACK is received for a transmitted packet.
 * Calling format	: ./tcpd_m2
 ****************************************************************************************************************************************/

/****************************************************
 * Header files
 ***************************************************/
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>			// Internet protocol family. Defines the sockaddr structure, SOCK_DGRAM/SOCK_STREAM
#include <arpa/inet.h>			// Contains the definitions for the internet operations (htonl, htons, ntohl, ntohs)
#include <netinet/tcp.h>		// Contains definitions for TCP

/****************************************************
 * Constants declaration
 ***************************************************/
#define TCPD_M2_PORT_IN 1051
#define LOCAL_IP "127.0.0.1"
#define TCP_HEADER_SIZE 20
#define MSS_SIZE 1000
#define TCPD_M2_PORT_OUT 1053
#define TCPD_M1_PORT_IN 1054
#define TCPD_M2_TIMER_OUT 1052
#define END_MESSAGE "##$$CONNECTEND$$##"
#define OUT_PSEUDO_SIZE 516
#define TIMEVAL2UNSIGNED(XX) ((XX.tv_sec*1000) + ((XX.tv_usec + 500)/1000))
#define WINDOW 20
#define IN_PSUEDO_SIZE 18
#define BIT20 0x80000
#define SEND_BUFFER 64
#define TRUE 1
#define FALSE 0
#define PASSED 0
#define FAILED 1
#define MSL 15

/****************************************************
 * RTO calculation method
 ****************************************************
 Err = M - A;
 A = A + g*Err;
 D = D + h (abs(Err) - D);
 RTO = A + 4D;

 A - smoothed RTT (an estimator of the avg)
 M - is the RTT (calculated after receiving every packet)
 D - smoothed mean deviation
 ***************************************************/

const float g = 0.125;
const float h = 0.25;

float Err, A = 0;									// Initial value of A has to be 0
unsigned long int M = 0;
float D = 3000;										// Initial value has to be 3
unsigned int RTO = 6000;							// RTO is initially set to 6000 milliseconds


unsigned int         	TCPDM2_in_socket;   		// Server in socket descriptor
struct to_troll_message troll_msg;
struct sockaddr_in   	TCPDM2_in_addr;     		// Server1 Internet address
unsigned int         	TCPDM2_out_socket;  		// Server out socket descriptor
struct sockaddr_in 	 	TCPDM2_out_addr;
unsigned int         	TCPDM2_timer_socket;  		// Server out socket descriptor
struct sockaddr_in   	TCPDM2_timer_addr;     		// Server1 Internet address
struct sockaddr_in   	ftpc_addr;
int 					fin_seq=0;
char 					fin_recvd=0;



/****************************************************
 * Data Structure declaration
 ***************************************************/

/****************************************************
 * Modes for the timer
 ***************************************************/
enum
{
	ESTABLISHED =0,
	FIN_WAIT1,
	FIN_WAIT2,
	TIME_WAIT,
	CLOSED,
}wrap_up_mode = 0;

/****************************************************
 * Modes for the timer
 ***************************************************/
enum timer_modes{
	INSERT_MODE=0,
	DELETE_MODE,
	END_MODE,
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
	unsigned int start, count;
	unsigned int ack_map:WINDOW;
	unsigned int sent, seq_no;
	unsigned int payload_size[SEND_BUFFER];
	struct timeval sent_time[SEND_BUFFER];
	struct timeval received_time[SEND_BUFFER];
	struct packet pack[SEND_BUFFER];

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
 * Prints the current Sliding window size
 ***************************************************/
void print_window()
{
	int i=0;
	printf("start:%d ",cb.start);
	for(i=WINDOW-1;i>=0;i--)
		printf("%u",(cb.ack_map>>i)&1);

	printf(" end:%d\n",(cb.sent -1));
}
/****************************************************
 * Function		: cbInit
 * Description	: Initializes the CB
 ***************************************************/
void cbInit()
{
	cb.start = 0;
	cb.count = 0;
	cb.ack_map = 0x00000;
	cb.sent = 0;
	cb.seq_no = 0;
}

/****************************************************
 * Function		: cbIsFull
 * Description	: Returns true if the CB is full
 ***************************************************/
int cbIsFull()
{
	if(cb.count>=SEND_BUFFER)
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
	if(cb.count == 0)
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
	cb.count--;
	cb.start = (cb.start + 1);
}

/****************************************************
 * Function		: cbpacket_exists
 * Description	: Returns true if a packet exists in a CB
 ***************************************************/
int cbpacket_exists(int index)
{
	if(index >= cb.start && index < (cb.start + cb.count))
		return TRUE;
	else
		return FALSE;
}


/****************************************************
 * Function		: cbWrite
 * Description	: Inserts the packet passed as argument
 * 				  into the CB
 ***************************************************/
int cbWrite(struct packet p)
{
	if(cbIsFull()==FALSE){
		memcpy(&cb.pack[(cb.start + cb.count)%SEND_BUFFER], &p, sizeof(struct packet));
		cb.count = cb.count + 1;
		return PASSED;
	}
	else
		return FAILED;
}

/****************************************************
 * Function		: compute_checksum_out
 * Description	: Compute the checksum for the packet sent from M1
 ***************************************************/
void compute_checksum_out(struct packet *p, struct sockaddr_in const * dest)
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
		unsigned short shrt_cst[OUT_PSEUDO_SIZE];
	} chk_struct;
	//	struct sockaddr_in src;
	socklen_t addr_len;
	int index;
	u_short sum = 0;

	addr_len = sizeof(struct sockaddr_in);
	bzero((void*)&chk_struct, sizeof(chk_struct));
	memcpy(&chk_struct.tcp_pseudohdr.tcp_packet,p,sizeof(struct packet));

	chk_struct.tcp_pseudohdr.dest_addr = inet_addr(LOCAL_IP);  			  	//dest->sin_addr.s_addr;
	//getsockname(TCPDM2_in_socket, (struct sockaddr *)&src,&addr_len);
	//printf("Calculated self address %s\n", inet_ntoa(src.sin_addr));
	chk_struct.tcp_pseudohdr.src_addr = inet_addr(LOCAL_IP);  			  	//src.sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.protocol = 6;
	chk_struct.tcp_pseudohdr.length_tcp = sizeof(struct packet);

	for(index=0;index<OUT_PSEUDO_SIZE;index++)
		sum = sum + chk_struct.shrt_cst[index];

	sum = ~sum;
	p->hdr.th_sum = sum;
}

/****************************************************
 * Function		: compute_checksum_in
 * Description	: Compute the checksum for the packet received from Troll
 ***************************************************/
u_short compute_checksum_in(struct tcphdr *hdr, struct sockaddr_in  const * src)
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
		unsigned short shrt_cst[IN_PSUEDO_SIZE];
	}chk_struct;

	u_short sum = 0;
	int index;
	struct sockaddr_in dest;
	socklen_t addr_len;

	addr_len = sizeof(dest);

	bzero((void*)&chk_struct, sizeof(chk_struct));
	memcpy(&chk_struct.tcp_pseudohdr.hdr,(void*)hdr,sizeof(struct tcphdr));


	chk_struct.tcp_pseudohdr.hdr.th_sum = 0;
	//getsockname(TCPDM2_in_socket, (struct sockaddr *)&dest,&addr_len);
	chk_struct.tcp_pseudohdr.dest_addr = inet_addr(LOCAL_IP);  			  	//dest.sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.src_addr = inet_addr(LOCAL_IP);  			  	//src->sin_addr.s_addr;
	chk_struct.tcp_pseudohdr.protocol = 6;
	chk_struct.tcp_pseudohdr.length_tcp = sizeof(struct tcphdr);

	for(index=0;index<IN_PSUEDO_SIZE;index++)
		sum = sum + chk_struct.shrt_cst[index];

	sum = ~sum;
	return sum;
}


/****************************************************
 * Function		: start_Time
 * Description	: Start the timer before sending a packet
 ***************************************************/
void start_Time(int index)
{
	gettimeofday(&cb.sent_time[index],NULL);
}


/****************************************************
 * Function		: end_Time
 * Description	: Stop the timer after receiving an ACK
 ***************************************************/
/*  */
void end_Time(int index)
{
	gettimeofday(&cb.received_time[index],NULL);
}


/****************************************************
 * Function		: calculate_RTT
 * Description	: To calculate the RTT for every packet
 ***************************************************/
int calculate_RTT(int index)
{
	index = index%SEND_BUFFER;
	end_Time(index);
	M = (TIMEVAL2UNSIGNED(cb.received_time[index])) -
			(TIMEVAL2UNSIGNED(cb.sent_time[index]));

	return M;
}


/****************************************************
 * Function		: calculate_RTO
 * Description	: This is for packets after the initial SYN.
 * 				  Only segments containing data are ACKed
 ***************************************************/
void calculate_RTO(unsigned long int M)
{
	static int call=0;
	if(call==0)
	{
		A = M + 500.0;
		D = A/2;
		RTO = A + 2*D;
		call++;
	}
	else{
		Err = M - A;
		A = A + g*Err;
		D = D + h * (abs(Err) - D);
		RTO = A + 4*D + 2000;
	}
}


/****************************************************
 * Function		: send_data_troll
 * Description	: Send data to the troll
 ***************************************************/
int send_data_troll(unsigned int index)
{
	struct timer_send_struct timer_data;
	struct in_addr addr;
	/*START:Calculate the Expected Return time for the packet and send to timer*/
	index = index % SEND_BUFFER;
	start_Time(index);

	/* Listen for the ACK and then stop the timer. Calculate the RTT from the start and end timings */
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
	printf("TO_TIMER: INSERT sequence data %u from tcpd_m2 to timer sec %lu usec %lu timesec %lu usec %lu\n",
			timer_data.seq_no,timer_data.AERT.tv_sec,timer_data.AERT.tv_usec,
			cb.sent_time[index].tv_sec, cb.sent_time[index].tv_usec);
	/*END:Calculate the Expected Return time for the packet*/

	/*START:Send data to troll, send only what was received*/
	memcpy(troll_msg.body,&cb.pack[index],sizeof(struct packet));
	if(sendto(TCPDM2_out_socket, (void*)&troll_msg, (sizeof(struct sockaddr_in) + TCP_HEADER_SIZE
			+ cb.payload_size[index]), 0, (struct sockaddr *)&TCPDM2_out_addr, sizeof(TCPDM2_out_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TROLL\n");
		return FAILED;
	}
	addr.s_addr = TCPDM2_out_addr.sin_addr.s_addr;
	printf("TO_TROLL:sequence %u %d\n",timer_data.seq_no,(sizeof(struct sockaddr_in) + TCP_HEADER_SIZE
			+ cb.payload_size[index]));

	/*END:Send data to troll*/
	return PASSED;
}


/****************************************************
 * Function		: window_change
 * Description	: For the sliding window
 ***************************************************/
int window_change()
{
	/*START: The send window might not be full, send data in that case*/
	if(((cb.sent - cb.start) < WINDOW) && cbpacket_exists(cb.sent))
	{
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
		if (cbpacket_exists(cb.sent))
		{
			if(send_data_troll(cb.sent)==FAILED)
				return FAILED;
			cb.sent++;
		}
		/*START:As ACKs were received, send new packets modify the window*/
	}
	/*END:Probably some ACKs were received, check if first packet in window ACKed*/
	print_window();
	return PASSED;
}


/****************************************************
 * Function		: send_finack
 * Description	: Send the FIN and ACK to terminate connection
 ***************************************************/
int send_finack(struct tcphdr *hdr)
{
	struct tcphdr finack_hdr;
	struct to_troll_message fin_msg;
	struct packet p;

	bzero((void*)&p, sizeof(p));
	finack_hdr.th_sport = TCPD_M1_PORT_IN;
	finack_hdr.th_dport = TCPD_M2_PORT_OUT;
	finack_hdr.th_ack = hdr->th_seq+1;
	finack_hdr.th_flags = TH_ACK;
	fin_msg.dest = troll_msg.dest;
	memcpy(&p,&finack_hdr, sizeof(finack_hdr));
	compute_checksum_out(&p,&fin_msg.dest);
	memcpy(fin_msg.body,&p,sizeof(p));

	if(sendto(TCPDM2_out_socket, (void*)&fin_msg, sizeof(struct to_troll_message), 0,
			(struct sockaddr *)&TCPDM2_out_addr, sizeof(TCPDM2_out_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TROLL\n");
		return FAILED;
	}
	return PASSED;
}

/****************************************************
 * Function		: process_ftpc_data
 * Description	: Process the data received from FTPC
 ***************************************************/
int process_ftpc_data()
{

	int bytes_recv_ftpc;
	struct in_addr addr;
	struct packet p;
	socklen_t addr_len;

	addr_len = sizeof(struct sockaddr_in);

	if(!cbIsFull())
	{
		bzero((void*)&p, sizeof(p));
		/*START: Receive data for the file and add it to buffer*/
		bytes_recv_ftpc = recvfrom(TCPDM2_in_socket, p.payload, MSS_SIZE, 0,
				(struct sockaddr *)&ftpc_addr, &addr_len);
		addr.s_addr = ftpc_addr.sin_addr.s_addr;

		/*START:Check if last message from ftpc, if yes then it would be a fin packet*/
		if(strcmp(p.payload,END_MESSAGE)==0 && wrap_up_mode == ESTABLISHED)
		{
			fin_seq = cb.seq_no;
			p.hdr.th_flags = p.hdr.th_flags | TH_FIN;
			printf("Received end msg entering finwait1\n");
			wrap_up_mode = FIN_WAIT1;
		}
		/*END:Check if last message from ftpc*/

		/*START:Packetize the data before adding it to the buffer*/
		p.hdr.th_sport = TCPD_M1_PORT_IN;
		p.hdr.th_dport = TCPD_M2_PORT_OUT;
		p.hdr.th_seq = cb.seq_no;
		compute_checksum_out(&p,&troll_msg.dest);
		printf("FTPC: alloc seq no %u\n", p.hdr.th_seq);

		/*START:The bytes actually requested might never be received, to make sure
		 * size sent to troll is what was received from ftpc*/
		cb.payload_size[(cb.start + cb.count)%SEND_BUFFER] = bytes_recv_ftpc;
		/*START:to make sure size sent is what was received */

		if(cbWrite(p)==FAILED)
			printf("Writing %u FAILED\n",p.hdr.th_seq );
		cb.seq_no ++;
		/*END:Packetize the data before adding it to the buffer*/
		/*END: Receive data for the file and add it to buffer*/
		window_change();
	}
	return PASSED;
}


/****************************************************
 * Function		: process_timer_data
 * Description	: Process any time outs sent by timer
 ***************************************************/
int process_timer_data()
{
	socklen_t addr_len;
	int bytes_recv_timer;
	unsigned int seq_no;
	/*START: Receive Timeouts for re transmission*/
	addr_len = sizeof(struct sockaddr_in);
	bytes_recv_timer = recvfrom(TCPDM2_timer_socket, &seq_no, sizeof(unsigned int), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, &addr_len);
	printf("TIMER: %d timed out\n",seq_no);
	/*END: Receive ACKs*/
	send_data_troll(seq_no);
	return PASSED;
}


/****************************************************
 * Function		: process_troll_data
 * Description	: Process ACKS/FINS from tcpd_m1
 ***************************************************/
int process_troll_data()
{
	struct from_troll_message troll_msg;
	struct timer_send_struct timer_data;
	struct tcphdr *hdr;
	unsigned int seq_number;
	unsigned short chksum;
	int bytes_recv_troll;
	socklen_t addr_len;

	/*START: Receive ACKs*/
	addr_len = sizeof(struct sockaddr_in);
	bytes_recv_troll = recvfrom(TCPDM2_out_socket, &troll_msg, sizeof(struct from_troll_message), 0,
			(struct sockaddr *)&TCPDM2_out_addr, &addr_len);
	/*END: Receive ACKs*/

	/*START: Compute Checksum, if not matched discard*/
	hdr = (struct tcphdr *)troll_msg.body;
	chksum = compute_checksum_in(hdr,&TCPDM2_out_addr);
	if(chksum != hdr->th_sum)
	{
		printf("TROLL: recv %u != calc %u, not processing\n",hdr->th_sum,chksum);
		return PASSED;
	}
	/*END: Compute Checksum, if not matched discard*/

	/*START: If fin is received send ack */
	if(wrap_up_mode >= FIN_WAIT2 && ((hdr->th_flags & TH_FIN )!=0))
	{
		fin_recvd = TRUE;
		if(cbIsEmpty()==TRUE)
		{
			if(send_finack(hdr)==FAILED)
				return FAILED;
			wrap_up_mode = TIME_WAIT;
		}
		return PASSED;
	}
	/*START: If fin is received send ack */

	/*START: Discard ACK if not in the window*/
	seq_number = hdr->th_ack - 1;
	printf("TROLL: seq %u acked to tcpd_m2\n\n",seq_number);
	if(seq_number<cb.start||seq_number>(cb.sent-1))
		return PASSED;
	/*END: Discard ACK if not in the window*/

	M = calculate_RTT(seq_number);

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

	/*START:See if the FIN packet was acked*/
	if(seq_number == fin_seq && wrap_up_mode == FIN_WAIT1)
	{
		printf("Entering FIN wait 2\n");
		wrap_up_mode = FIN_WAIT2;
	}
	/*END:See if the FIN packet was acked*/

	/*START:Modify the window*/
	cb.ack_map = cb.ack_map|(1<<(((WINDOW - (seq_number - cb.start))) - 1));
	window_change();
	/*END:Modify the window*/

	if(cbIsEmpty()==TRUE&&fin_recvd==TRUE)
	{
		if(send_finack(hdr)==FAILED)
			return FAILED;
		printf("Sent ack to FIN entering Timewait\n");
		wrap_up_mode = TIME_WAIT;
	}
	return PASSED;
}


int main(int argc, char *argv[])
{
	int ret = 0;
	struct timeval msl2;
	unsigned int 	highest_fd=0;
	struct timer_send_struct timer_data;
	fd_set 					TCPDM2_in_set;
	socklen_t addr_len;

	if(argc!=1)
	{
		perror("Format tcpd_m2\n");
		return 1;
	}

	cbInit();

	/*START:Establish connection as a server with ftpc as a client on port 1051*/
	TCPDM2_in_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM2_in_addr.sin_family      = AF_INET;            				// Address family to use
	TCPDM2_in_addr.sin_port        = htons(TCPD_M2_PORT_IN);    		// Port number to use
	TCPDM2_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);  				// Listen on any IP address
	if(bind(TCPDM2_in_socket, (struct sockaddr *)&TCPDM2_in_addr, sizeof(TCPDM2_in_addr))!=0)
	{
		perror("Binding failure tcpd_m2\n");
		return 1;
	}
	if(TCPDM2_in_socket>highest_fd)
		highest_fd = TCPDM2_in_socket;
	/*END:Establish connection as a server with ftpc as a client on port 1051*/

	/*START: Receive details of the address of ftps*/
	if( recvfrom(TCPDM2_in_socket, &troll_msg.dest, sizeof(struct sockaddr_in), 0,
			(struct sockaddr *)&ftpc_addr, &addr_len)==-1)
	{
		perror("Couldn't receive ftps details\n");
		return 1;
	}
	/*END: Receive details of the address of FTPS*/

	/*START:Establish connection as a client with TROLL on port 1053*/
	TCPDM2_out_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM2_out_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_out_addr.sin_port        = htons(TCPD_M2_PORT_OUT);    // Port number to use
	TCPDM2_out_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);// Send to passed IP address
	if(TCPDM2_out_socket>highest_fd)
		highest_fd = TCPDM2_out_socket;
	/*END:Establish connection as a client with TROLL on port 1053*/

	/*START:receive destination details and configure for troll to send it to this addr*/
	troll_msg.dest.sin_family      = AF_INET;            			// Address family to use
	troll_msg.dest.sin_port        = htons(TCPD_M1_PORT_IN);  // Port number to use
	/*END:receive destination details and configure for troll to send it to this addr*/

	/*START:Establish connection as a client with timer on port 1052*/
	TCPDM2_timer_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TCPDM2_timer_addr.sin_family      = AF_INET;            			// Address family to use
	TCPDM2_timer_addr.sin_port        = htons(TCPD_M2_TIMER_OUT);    // Port number to use
	TCPDM2_timer_addr.sin_addr.s_addr = inet_addr(LOCAL_IP);  					// Send to passed IP address
	if(TCPDM2_timer_socket>highest_fd)
		highest_fd = TCPDM2_timer_socket;
	/*END:Establish connection as a client with timer on port 1052*/

	/*START:Increment the highest FD by 1 for use with select*/
	highest_fd++;
	/*END:Increment the highest FD by 1 for use with select*/

	/*START: Prepare 2MSL time wait structure*/
	msl2.tv_sec = MSL*2;
	msl2.tv_usec = 0;
	/*END: Prepare 2MSL time wait structure*/

	while(1)
	{
		FD_ZERO(&TCPDM2_in_set);
		FD_SET(TCPDM2_timer_socket,&TCPDM2_in_set);
		FD_SET(TCPDM2_in_socket,&TCPDM2_in_set);
		FD_SET(TCPDM2_out_socket,&TCPDM2_in_set);

		/*START:Listen on all connections*/
		if(wrap_up_mode==TIME_WAIT)
			ret = select(highest_fd, &TCPDM2_in_set, NULL, NULL, &msl2);
		else
			ret = select(highest_fd, &TCPDM2_in_set, NULL, NULL, NULL);
		/*END:Listen on all connections*/

		if (ret == -1)
		{
			perror("Select failure\n");
			return 1;
		}

		/*START: If received from troll*/
		if (FD_ISSET(TCPDM2_out_socket, &TCPDM2_in_set))
		{
			if (process_troll_data()!=PASSED)
			{
				perror("Failed TROLL process\n");
				return 1;
			}
		}
		/*START: If received from troll*/

		/*START: If received from timer*/
		else if (FD_ISSET(TCPDM2_timer_socket, &TCPDM2_in_set))
		{
			if (process_timer_data()!=PASSED)
			{
				perror("Failed timer process\n");
				return 1;
			}
		}
		/*END: If received from timer*/

		/*START: If received from ftpc*/
		else if (FD_ISSET(TCPDM2_in_socket, &TCPDM2_in_set))
		{
			if(cbIsFull()!=TRUE)
			{
				if (process_ftpc_data()!=PASSED)
				{
					perror("Failed FTPC process\n");
					return 1;
				}
			}
			else
				printf("Warning: Buffer Full might lose some packets!!!\n");

		}
		/*END: If received from ftpc*/

		else
			break;
		/*END: If received from ftpc*/
	}

	wrap_up_mode = CLOSED;
	/*START: Tell timer "Dude it's time to end*/
	timer_data.mode = END_MODE;
	if(sendto(TCPDM2_timer_socket, (void*)&timer_data, sizeof(struct timer_send_struct), 0,
			(struct sockaddr *)&TCPDM2_timer_addr, sizeof(TCPDM2_timer_addr))==-1)
	{
		perror("Cannot send data from TCPD_M1 to TIMER\n");
		return FAILED;
	}
	/*END: Tell timer "Dude it's time to end*/

	close(TCPDM2_in_socket);
	close(TCPDM2_out_socket);
	close(TCPDM2_timer_socket);
	return 0;
}
