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

/****************************************************
 * Constants declaration
 ***************************************************/
#define BUFFER_SIZE 1000
#define END_MESSAGE "##$$CONNECTEND$$##"
#define PORT 1055

//Format ftpc <remote-IP-address> <remote-port-number> <local-file-to-transfer> <TCPD-M2 IP>

extern char TCPDM2_IP[16];
extern unsigned int TCPDM2_socket;

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
	unsigned int         client_s;        // client socket descriptor
	struct sockaddr_in   server_addr;     // Server Internet address
	char                 out_buf[BUFFER_SIZE];    // 1000-byte output buffer for data
	int                  ret;             // return value
	FILE                 *fp;
	unsigned int         bytes_read;
	unsigned int sz;
	char 								end_msg[12];

	struct metadata
	{
		unsigned int size;
		char name[20];
	}out_meta;

	if(argc!=5)
	{
		perror("Format ftpc <remote-IP-address> <remote-port-number> <local-file-to-transfer> <TCPD-M2 IP>\n");
		return 1;
	}
	strcpy(end_msg,END_MESSAGE);
	client_s = socket(AF_INET, SOCK_STREAM, 0);
	strcpy(TCPDM2_IP,argv[4]);

	/*START:Fill-in the client socket's address and connect with server*/
	/*server_addr.sin_family      = AF_INET;            // Address family to use
	server_addr.sin_port        = htons(atoi(argv[2]));    // Port num to use
	server_addr.sin_addr.s_addr = inet_addr(argv[1]); // IP address to use
	ret = connect(client_s, (struct sockaddr *)&server_addr, sizeof(server_addr));

	if (ret!=0)
	{
		perror("Connect failure\n");
		return 1;
	}*/
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
	sz = getsize(fp);
	if(SEND(client_s, &sz, sizeof(int), 0)==-1)
		return 1;
	if(SEND(client_s, argv[3], 20, 0)==-1)
		return 1;
	/*END:Prepare and send the metadata of the file*/

	/*START:Read contents of and send using RECV*/
	while(!feof(fp))
	{
		bytes_read = fread(out_buf, 1, BUFFER_SIZE, fp);
		if(SEND(client_s, out_buf, bytes_read, 0)==-1)
			return 1;
	}
	/*START:send END message*/
	strcpy(out_buf,END_MESSAGE);
	if(SEND(client_s, out_buf, strlen(END_MESSAGE)+1, 0)==-1)
		return 1;
	/*END:send END message*/
	printf("File Sent\n");
	/*END:Read contents of and send using RECV*/
	fclose(fp);
	close(client_s);
	close(TCPDM2_socket);
	return 0;
}
