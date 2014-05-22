#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tcpd_socket.h"
#define BUFFER_SIZE 1000
#define PORT 1055
#define FILE_NAME_SIZE 20
//Format ftps <local-port-number> <TCPD_M1 IP>

extern char TCPDM1_IP[16];
extern unsigned int TCPDM1_socket;

int main(int argc, char *argv[])
{
	unsigned int         server_s;        // Server socket descriptor
	struct sockaddr_in   server_addr;     // Server Internet address
	unsigned int         connect_s;       // Connection socket descriptor
	struct sockaddr_in   client_addr;     // Client Internet address
	struct in_addr       client_ip_addr;  // Client IP address
	int                  addr_len;        // Internet address length
	char                 in_buf[BUFFER_SIZE];     // BUFFER_SIZE-byte input buffer for data
	unsigned int         bytes_recv=0;    // Gauge number of bytes received
	unsigned int         total_size=0;
	int 									remaining;
	FILE *fp;
	struct metadata
	{
		unsigned int size;
		char name[20];
	} in_meta;

	if(argc!=2)
	{
		perror("Format ftps <local-port-number> \n");
		return 1;
	}

	/*strcpy(TCPDM1_IP,argv[2]);
	server_s = socket(AF_INET, SOCK_STREAM, 0);*/

	/*START:Fill-in my socket's address information and bind the socket*/
	/*server_addr.sin_family      = AF_INET;            // Address family to use
	server_addr.sin_port        = htons(atoi(argv[1]));    // Port number to use
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address

	if(bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr))!=0)
	{
		perror("Binding failure \n");
		return 1;
	}*/
	/*END:Fill-in my socket's address information and bind the socket*/

	/*START:Establish TCP connection with the client*/
	/*
	if(listen(server_s, 1)!=0)
	{
		perror("Listening failure \n");
		return 1;
	}
	addr_len = sizeof(client_addr);
	if((connect_s = accept(server_s, (struct sockaddr *)&client_addr, &addr_len))==-1)
	{
		perror("Accept failure \n");
		return 1;
	}*/
	/*END:Establish TCP connection with the client*/

	/*START:Start Receiving meta data using UDP RECV*/
	bytes_recv = RECV (connect_s, in_buf, BUFFER_SIZE , 0); //receive the metadata of the file
	if(bytes_recv==-1)
			return 1;
	memcpy(&in_meta.size,in_buf,sizeof(int));
	printf("size is %d\n",in_meta.size);
	remaining = in_meta.size;

	bytes_recv = RECV (connect_s, in_buf, BUFFER_SIZE, 0); //receive the metadata of the file
	if(bytes_recv==-1)
			return 1;
	memcpy(in_meta.name,in_buf,FILE_NAME_SIZE);
	printf("Name is %s\n",in_meta.name);
	/*END:Start Receiving meta data using UDP RECV*/

	/*START:Create a file with received name*/
	fp=fopen(in_meta.name,"w");
	if(fp==NULL)
	{
		perror("Cannot Create file");
		return 1;
	}
	/*END:Create a file with received name*/

	/*START:Receive file data using UDP RECV*/
	while(1)
	{
		bytes_recv = RECV (connect_s, in_buf, BUFFER_SIZE , 0);
		if(bytes_recv==-1)
				return 1;
		if (remaining<=0)//||bytes_recv<=0)
			break;
		else
			remaining = remaining - fwrite(in_buf, 1, (remaining<BUFFER_SIZE?remaining:BUFFER_SIZE) , fp);
	}
	printf("File Received %s\n",in_meta.name);
	/*START:Receive file data using UDP RECV*/

	fclose(fp);
	close(connect_s);
	close(server_s);
	close(TCPDM1_socket);
	return 0;

}

