/****************************************************
 * Authors: Abhishek and Ananth
 * Date:
 ****************************************************
 * File: TCPD_M2.c
 * Description:
 ***************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "tcpd_socket.h"

/****************************************************
 * Constants declaration
 ***************************************************/
#define BUFFER_SIZE 1000
#define FTPS_PORT 1055
#define FILE_NAME_SIZE 20

/*START:For TCPDM1*/
extern struct sockaddr_in TCPDM1_addr;
/*END:For TCPDM1*/

//Format ftps

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int main(int argc, char *argv[])
{
	int call = 0;
	unsigned int         server_s;        // Server socket descriptor
	unsigned int         connect_s;       // Connection socket descriptor
	char                 in_buf[BUFFER_SIZE];     // BUFFER_SIZE-byte input buffer for data
	unsigned int         bytes_recv=0;    // Gauge number of bytes received
	int 								remaining;
	struct sockaddr_in server_addr;
	socklen_t addr_len;
	struct sockaddr_in client_addr;
	FILE *fp;

	struct
	{
		unsigned int size;
		char name[20];
	} in_meta;

	if(argc!=2)
	{
		perror("Format ftps <Port-num>\n");
		return 1;
	}


	server_s = SOCKET(AF_INET, SOCK_STREAM, 0);

	/*START:Fill-in my socket's address information and bind the socket*/
	server_addr.sin_family      = AF_INET;            // Address family to use
	server_addr.sin_port        = htons(atoi(argv[1]));    // Port number to use
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address

	if(BIND(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr))!=0)
	{
		perror("Binding failure \n");
		return 1;
	}
	/*END:Fill-in my socket's address information and bind the socket*/

	/*START:Establish TCP connection with the client*/
	if((connect_s = ACCEPT(server_s, (struct sockaddr *)&client_addr, &addr_len))==-1)
	{
		perror("Accept failure \n");
		return 1;
	}
	/*END:Establish TCP connection with the client*/

	/*START:Start Receiving meta data using RECV*/
	bytes_recv = RECV (connect_s, in_buf, BUFFER_SIZE , 0); //receive the metadata of the file
	if(bytes_recv==-1)
	{
		perror("Error receiving");
		return 1;
	}
	printf("count:%u\n",call);
	call++;
	memcpy(&in_meta,in_buf,sizeof(in_meta));
	printf("size is %d\n",in_meta.size);
	remaining = in_meta.size;
	printf("Name is %s\n",in_meta.name);
	/*END:Start Receiving meta data using RECV*/

	/*START:Create a file with received name*/
	fp=fopen(in_meta.name,"w");
	if(fp==NULL)
	{
		perror("Cannot Create file");
		return 1;
	}
	/*END:Create a file with received name*/

	/*START:Receive file data using UDP RECV*/
	while(remaining>0)
	{
		bytes_recv = RECV (connect_s, in_buf, BUFFER_SIZE , 0);
		if(bytes_recv==-1)
		{
			perror("Error receiving");
			return 1;
		}
		printf("count:%u\n",call);
		call++;
		remaining = remaining - fwrite(in_buf, 1,(remaining<BUFFER_SIZE?remaining:BUFFER_SIZE) , fp);
	}
	printf("File Received %s\n",in_meta.name);
	/*START:Receive file data using UDP RECV*/

	fclose(fp);
	CLOSE(connect_s);
	CLOSE(server_s);
	return 0;
}

