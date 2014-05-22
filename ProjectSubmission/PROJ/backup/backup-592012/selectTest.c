/*1. open 2 sockets - one for the client and one for the troll.
2. create a master fd_set and add the client's socket to it
3. call select
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MY_PORT 12000
#define TIMER_PORT 13000
#define TROLL_PORT 14000
#define BUFFER_SIZE 256

int main(void)
{
	int fdmax; 			/*file descriptor*/
	int listener;		/*listening socket*/
	int reader;			/*2 other sockets*/
	int writer;
	fd_set master;		/*master file descriptor list*/
	fd_set read_fds;	/*temp file descriptor*/
	struct sockaddr_in 	my_addr, timer_addr, troll_addr, client_addr;
	struct sockaddr_storage remoteaddr;
	int yes = 1;		/*for setsockopt() SO_REUSEADDR*/
	size_t resultRecv=0;
	char BUF[BUFFER_SIZE];
	int i=0,addrLen = 0;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(MY_PORT);

	timer_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	timer_addr.sin_family = AF_INET;
	timer_addr.sin_port = htons(TIMER_PORT);

	troll_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	troll_addr.sin_family = AF_INET;
	troll_addr.sin_port = htons(TROLL_PORT);

	/*create a socket*/
	if((listener = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Error opening datagram socket\n");
		return(1);
	}

	reader = socket(AF_INET, SOCK_DGRAM, 0);
	FD_SET(reader, &master);
	writer = socket(AF_INET, SOCK_DGRAM, 0);
	FD_SET(writer, &master);

	/*To get rid of the "address already in use" error message */
	if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR, &yes, sizeof(int)) < 0)
	{
		perror("Error while using setsockopt\n");
		return(1);
	}

	/*Bind the socket to listen for msg*/
	if(bind(listener,(struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
	{
		perror("Error binding on socket-listener");
		return(1);
	}

	/*Add the listener to the master set*/
	FD_SET(listener,&master);

	/*Keep track of the biggest file descriptor*/
	fdmax = listener>reader?listener:reader;
	fdmax = fdmax>writer?fdmax:writer;
	fdmax++;

	printf("Listening\n");

	/*main loop*/
	for(;;)									/*Start of main for loop*/
	{
		read_fds = master;
		printf("Inside main for loop\n");
		if(select(fdmax, &master, NULL, NULL, NULL) == -1)
		{
			perror("Error in select()\n");
			return(1);
		}

		/*Run through the connections looking for data*/
		addrLen = sizeof(client_addr);
		for(i=0; i<=fdmax; i++)
		{
			if(FD_ISSET(i, &read_fds))
			{
				printf("after FD_ISSET\n");
				if((resultRecv = recvfrom(listener, &BUF, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addrLen)) < 0)
				{
					perror("Error in recvFrom\n");
					return(1);
				}
				else
				{
					printf("the received data is: %s\n", BUF);
				}
			}
		}										/*End of connection looking for loop*/
	}										/*End of main for loop*/

	printf("Outside main for loop\n");


}											/*End of main*/
