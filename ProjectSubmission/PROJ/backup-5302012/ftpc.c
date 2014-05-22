/****************************************************
 * Authors: Abhishek and Ananth
 * Date:
 ****************************************************
 * File: TCPD_M2.c
 * Description:
 ***************************************************/
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include "tcpd_socket.h"
#include <arpa/inet.h>
#include <stdlib.h>

/****************************************************
 * Constants declaration
 ***************************************************/
#define BUFFER_SIZE 1000
#define PORT 1055

/*START:For TCPDM2*/
extern struct sockaddr_in TCPDM2_addr;
/*END:For TCPDM2*/

//Format ftpc <remote-IP-address> <remote-port-number> <local-file-to-transfer>

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
/*START: Get the file size*/
unsigned int getsize(FILE *fp)
{
	unsigned int sz = 0;
	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	return sz;
}
/*END: Get the file size*/

/****************************************************
 * Function: cbInit
 * Description:
 ***************************************************/
int main(int argc, char *argv[])
{
	int call =0;
	unsigned int         client_s;        // client socket descriptor
	struct sockaddr_in   server_addr;     // Server Internet address
	char                 out_buf[BUFFER_SIZE];    // 1000-byte output buffer for data
	//int                  ret;             // return value
	FILE                 *fp;
	unsigned int         bytes_read;
	//unsigned int sz;
	char 								end_msg[12];
	int 	len;

	struct
	{
		unsigned int size;
		char name[20];
	}out_meta;

	if(argc!=4)
	{
		perror("Format ftpc <remote-IP-address> <remote-port-number> <local-file-to-transfer>\n");
		return 1;
	}
	strcpy(end_msg,END_MESSAGE);
	client_s = SOCKET(AF_INET, SOCK_STREAM, 0);

	/*START:Fill-in the client socket's address and connect with server*/
	server_addr.sin_family      = AF_INET;            // Address family to use
	server_addr.sin_port        = htons(atoi(argv[2]));    // Port num to use
	server_addr.sin_addr.s_addr = inet_addr(argv[1]); // IP address to use
	if (CONNECT(client_s, (struct sockaddr *)&server_addr, sizeof(server_addr))!=0)
	{
		perror("Connect failure\n");
		return 1;
	}
	/*END:Fill-in the client socket's address and connect with server*/

	/*START:Open file to read its contents*/
	fp = fopen(argv[3],"r");
	if(fp==NULL)
	{
		perror("Cannot read input file");
		return 1;
	}
	/*END:Open file to read its contents*/

	/*START:Prepare and send the metadata of the file*/
	out_meta.size = getsize(fp);
	strcpy(out_meta.name,argv[3]);
	if((len=SEND(client_s, &out_meta, sizeof(out_meta), 0))==-1)
	{
		printf("Sending error");
		return 1;
	}
	printf("Sent %u size data from ftpc to %s count %d\n",len,inet_ntoa(TCPDM2_addr.sin_addr),(call));
	call++;
	/*END:Prepare and send the metadata of the file*/

	/*START:Read contents of and send using RECV*/
	while(!feof(fp))
	{
		bytes_read = fread(out_buf, 1, BUFFER_SIZE, fp);
		if((len = SEND(client_s, out_buf, bytes_read, 0))==-1)
		{
			printf("Sending error");
			return 1;
		}
		printf("Sent %u size data from ftpc to %s count %d\n",len,inet_ntoa(TCPDM2_addr.sin_addr),(call));
		call++;
	}
	printf("File Sent\n");
	/*END:Read contents of and send using RECV*/
	fclose(fp);
	CLOSE(client_s);
	return 0;
}
