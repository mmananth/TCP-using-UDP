/****************************************************************************************************************************************
 * Authors	: Abhishek and Ananth
 * Date		: June-6-2012
 ****************************************************************************************************************************************
 * File				: TCPD_M2.c
 * Description		: This is the code for the client application that transfers the data to the local TCPD-M2 process.
 * 					  It first creates a connection with TCPD-M2 using the CONNECT() call, sends the file in 1000 byte chunks
 * 					  (MTU=1000 bytes) using the SEND() call.
 * Calling format	: ftpc <remote-IP-address> <remote-port-number> <local-file-to-transfer>; Eg.: ./ftpc 164.107.112.23 1054 test.txt
 ****************************************************************************************************************************************/

/****************************************************
 * Header files
 ***************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>		// Internet protocol family. Defines the sockaddr structure, SOCK_DGRAM/SOCK_STREAM
#include <arpa/inet.h>		// Contains the definitions for the internet operations (htonl, htons, ntohl, ntohs)
#include "tcpd_socket.h"	// Contains the declarations for the TCP function calls

/****************************************************
 * Constants declaration
 ***************************************************/
#define BUFFER_SIZE 1000	// For the packet size
#define PORT 1055

/*START:For TCPDM2*/
extern struct sockaddr_in TCPDM2_addr;
/*END:For TCPDM2*/

/****************************************************
 * Function		: getsize
 * Description	: Returns the size of the file object
 * 				  passed as the argument.
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


int main(int argc, char *argv[])
{
	int call = 0;
	unsigned int        client_s;        			// client socket descriptor
	struct sockaddr_in  server_addr;     			// Server Internet address
	char                out_buf[BUFFER_SIZE];    	// 1000-byte output buffer for data
	FILE                *fp;
	unsigned int        bytes_read;
	char 				end_msg[12];
	int 				len;

	struct
	{
		unsigned int size;
		char name[20];
	}out_meta;

	if(argc!=4){
		perror("Format ftpc <remote-IP-address> <remote-port-number> <local-file-to-transfer>\n");
		return 1;
	}
	strcpy(end_msg,END_MESSAGE);
	client_s = SOCKET(AF_INET, SOCK_STREAM, 0);

	/*START:Fill-in the client socket's address and connect with server*/
	server_addr.sin_family      = AF_INET;            		// Address family to use
	server_addr.sin_port        = htons(atoi(argv[2]));    	// Port number to use
	server_addr.sin_addr.s_addr = inet_addr(argv[1]); 		// IP address to use
	if (CONNECT(client_s, (struct sockaddr *)&server_addr,
									sizeof(server_addr))!=0){
		perror("Connect failure\n");
		return 1;
	}
	/*END:Fill-in the client socket's address and connect with server*/

	/*START:Open file to read its contents*/
	fp = fopen(argv[3],"r");
	if(fp==NULL){
		perror("Cannot read input file");
		return 1;
	}
	/*END:Open file to read its contents*/

	/*START:Prepare and send the metadata of the file*/
	out_meta.size = getsize(fp);
	strcpy(out_meta.name,argv[3]);
	if((len=SEND(client_s, &out_meta, sizeof(out_meta), 0))==-1){
		printf("Sending error");
		return 1;
	}
	printf("Sent %u size data from ftpc to %s count %d\n",len,inet_ntoa(TCPDM2_addr.sin_addr),(call));
	call++;
	/*END:Prepare and send the metadata of the file*/

	/*START:Read contents of and send using SEND*/
	while(!feof(fp))
	{
		bytes_read = fread(out_buf, 1, BUFFER_SIZE, fp);
		if((len = SEND(client_s, out_buf, bytes_read, 0))==-1){
			printf("Sending error");
			return 1;
		}
		printf("Sent %u size data from ftpc to %s count %d\n",len,inet_ntoa(TCPDM2_addr.sin_addr),(call));
		call++;
	}
	printf("File Sent\n");
	/*END:Read contents of and send using SEND*/
	fclose(fp);
	CLOSE(client_s);
	return 0;
}
