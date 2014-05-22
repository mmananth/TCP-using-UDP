#include<stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#define TIMER_PORT_IN 1052
#define TIMEVAL2UNSIGNED(XX) ((XX.tv_sec*1000) + (XX.tv_usec/1000))
typedef struct packet_node *PAC_NODE;

struct recv_struct
{
	unsigned int seq_no;
	struct timeval AERT;
	int mode;
};

unsigned int TIMER_in_socket;
struct timeval total_time_elapsed;
struct itimerval value, setvalue, ovalue;
struct sockaddr_in   TIMER_addr;
struct sockaddr_in   TCPDM2_addr;
struct timeval curr_time;
struct recv_struct from_tcpd;
int addr_len;
PAC_NODE head = NULL;


struct packet_node
{
	unsigned int seq_no;
	unsigned long delta_time;
	PAC_NODE next;
	PAC_NODE prev;
};

void timer_calibrate();

void insert_into()
{
	PAC_NODE node;
	PAC_NODE traverse;

	node = (PAC_NODE)malloc(sizeof(struct packet_node));

	/*START: Get absolute time of day and subtract AERT to get delta time*/
	if(head==NULL)
	{
		gettimeofday(&total_time_elapsed ,NULL);
	}
	node->delta_time = (from_tcpd.AERT.tv_sec - total_time_elapsed.tv_sec)*1000 +
			(from_tcpd.AERT.tv_usec - total_time_elapsed.tv_usec)/1000;
	/*END: Get absolute time of day and subtract AERT to get delta time*/

	node->seq_no = from_tcpd.seq_no;
	node->next = NULL;
	node->prev = NULL;

	/*START:If there are no elements in the list add node as head*/
	if(head==NULL)
	{
		head = node;
		//gettimeofday(&curr_time,NULL);
		timer_calibrate();
		return;
	}
	/*END:If there are no elements in the list add node as head*/

	/*START:look for node's place and place it there*/
	traverse = head;
	while(1)
	{
		if(node->delta_time<traverse->delta_time)
		{
			if(traverse->prev!=NULL)
				(traverse->prev)->next = node;
			node->prev = traverse->prev;
			node->next = traverse;
			traverse->prev = node;
			traverse->delta_time = traverse->delta_time - node->delta_time;
			if (traverse==head)
			{
				head = node;
				timer_calibrate();
			}
			break;
		}
		else if(traverse->next==NULL)
		{
			node->delta_time = node->delta_time - traverse->delta_time;
			traverse->next = node;
			node->prev = traverse;
			break;
		}
		else
		{
			node->delta_time = node->delta_time - traverse->delta_time;
			traverse = traverse->next;
		}

	}
	/*END:look for node's place and place it there*/

	return;
}
void delete_from()
{
	PAC_NODE traverse;
	traverse = head;
	while(traverse!=NULL)
	{
		if(traverse->seq_no==from_tcpd.seq_no)
		{
			if(traverse->next!=NULL)
			{
				(traverse->next)->prev = traverse->prev;
				(traverse->next)->delta_time = (traverse->next)->delta_time + traverse->delta_time;
			}
			if(traverse->prev!=NULL)
				(traverse->prev)->next = traverse->next;

			if(traverse == head)
			{
				if(traverse->next!=NULL)
					head = traverse->next;
				else
					head = NULL;
				timer_calibrate();
			}
			free(traverse);
			break;
		}
		else traverse = traverse->next;
	}
}

void print_list()
{
	PAC_NODE traverse;
	traverse = head;
	while(traverse!=NULL)
	{
		printf("Seq No:%u delta time:%u prev %u next %u own %u\n",traverse->seq_no,traverse->delta_time,traverse->prev,traverse->next,traverse);
		traverse = traverse->next;
	}
}
enum modes{
	INSERT_MODE=0,
	DELETE_MODE,
};

void time_elapsed()
{
	PAC_NODE	traverse;
	struct in_addr addr;
	if (head!=NULL)
	{
		/*START:Send the head sequence number to TCPD, for retransmission*/
		printf("Timer elapsed for %d\n",head->seq_no);
		if(sendto(TIMER_in_socket, (void*)&(head->seq_no), sizeof(unsigned int), 0,
				(struct sockaddr *)&TCPDM2_addr, sizeof(TCPDM2_addr))==-1)
		{
			perror("send problem");
		}
		addr.s_addr = TCPDM2_addr.sin_addr.s_addr;

		printf("Sent to TCPD_M2 %d to %s port %u\n",head->seq_no,inet_ntoa(addr),ntohs(TCPDM2_addr.sin_port));
		/*END:Send the head sequence number to TCPD, for retransmission*/

		/*START:Delete the head node*/
		total_time_elapsed.tv_sec = total_time_elapsed.tv_sec + head->delta_time/1000;
		total_time_elapsed.tv_usec = total_time_elapsed.tv_usec + (head->delta_time%1000)*1000;

		traverse = head;
		if(head->next==NULL)
			head=NULL;
		else
		{
			(head->next)->prev = NULL;
			head = head->next;
		}
		free(traverse);
		print_list();
		/*END:Delete the head node*/

		/*START: Call the timer generator*/
		if(head!=NULL)
			timer_calibrate();
		/*END: Call the timer generator*/
	}
}
void timer_calibrate()
{
	struct itimerval setvalue, ovalue, currvalue;

	setvalue.it_interval.tv_sec=0;
	setvalue.it_interval.tv_usec=0;

	if(head!=NULL)
	{
		setvalue.it_value.tv_sec=head->delta_time/1000;
		setvalue.it_value.tv_usec=(head->delta_time%1000)*1000;
	}
	else
	{
		setvalue.it_value.tv_sec=0;
		setvalue.it_value.tv_usec=0;
	}
	/*START:initialize timer*/
	printf("Timer set to trigger in sec %u usec %u\n",
			setvalue.it_value.tv_sec,setvalue.it_value.tv_usec);
	signal(SIGALRM, time_elapsed);
	setitimer(ITIMER_REAL, &setvalue, &ovalue);


	/*END:initialize timer*/

	//getitimer(ITIMER_REAL,&currvalue);
	//printf("Timer set to trigger in sec %u usec %u",currvalue.it_value.tv_sec,
	//currvalue.it_value.tv_usec);
}
int main()
{

	int bytes_recv;
	//struct recv_struct array[5];
	int i=0;

	/*array[0].seq_no = 0;
	array[0].AERT.tv_usec = ADD_TIME(total_elapsed_time,30);
	printf("seq %d, time %u\n",array[0].seq_no ,array[0].AERT.tv_usec);
	array[0].AERT.tv_sec = total_elapsed_time.tv_sec;
	array[0].mode =0;

	array[1].seq_no = 2;
	array[1].AERT.tv_usec = ADD_TIME(total_elapsed_time,40);
	printf("seq %d, time %u\n",array[1].seq_no, array[1].AERT.tv_usec);
	array[1].AERT.tv_sec = total_elapsed_time.tv_sec;
	array[1].mode =0;

	array[2].seq_no = 3;
	array[2].AERT.tv_usec = ADD_TIME(total_elapsed_time,20);
	array[2].AERT.tv_sec = total_elapsed_time.tv_sec;
	array[2].mode =0;

	array[4].seq_no = 3;
	array[4].AERT.tv_usec = ADD_TIME(total_elapsed_time,40);
	array[4].AERT.tv_sec = total_elapsed_time.tv_sec;
	array[4].mode =1;

	array[3].seq_no = 2;
	array[3].AERT.tv_usec = ADD_TIME(total_elapsed_time,40);
	array[3].AERT.tv_sec = total_elapsed_time.tv_sec;
	array[3].mode =1;*/

	/*START:Establish connection as a server with tcpd_m2 as a client on port 1052*/
	TIMER_in_socket = socket(AF_INET, SOCK_DGRAM, 0);
	TIMER_addr.sin_family      = AF_INET;            				// Address family to use
	TIMER_addr.sin_port        = htons(TIMER_PORT_IN);    // Port number to use
	TIMER_addr.sin_addr.s_addr = htonl(INADDR_ANY);  				// Listen on any IP address
	if(bind(TIMER_in_socket, (struct sockaddr *)&TIMER_addr, sizeof(TIMER_addr))!=0)
	{
		perror("Binding failure tcpd_m2\n");
		return 1;
	}
	/*END:Establish connection as a server with tcpd_m2 as a client on port 1052*/

	addr_len = sizeof(struct sockaddr_in);


	while(1)
	{
		//printf("Iteration %d\n",i);
		//from_tcpd = array[i];
		/*START:Recv data from TCPD*/
		struct in_addr addr;
		int ret;
		fd_set 								TIMER_in_set;
		FD_ZERO(&TIMER_in_set);
		FD_SET(TIMER_in_socket,&TIMER_in_set);
		ret = select(TIMER_in_socket+1, &TIMER_in_set, NULL, NULL, NULL);
		if (FD_ISSET(TIMER_in_socket, &TIMER_in_set))
		{
			bytes_recv = recvfrom(TIMER_in_socket,&from_tcpd, sizeof (from_tcpd), 0,
					(struct sockaddr *)&TCPDM2_addr, &addr_len);
			/*END:Recv data from TCPD*/
			addr.s_addr = TCPDM2_addr.sin_addr.s_addr;
			printf("Received seq %u sec %u usec from %u from %s %u mode %d\n"
					,from_tcpd.seq_no,from_tcpd.AERT.tv_sec,from_tcpd.AERT.tv_usec,
					inet_ntoa(addr),ntohs(TCPDM2_addr.sin_port),from_tcpd.mode);

			switch (from_tcpd.mode)
			{
			case INSERT_MODE:
				insert_into();
				break;
			case DELETE_MODE:
				delete_from();
				break;
			}
			print_list();
			printf("\n");
		}

	}
	/*START:Recv data from TCPD*/
	return 0;

}
