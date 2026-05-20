// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "PDU.h"

#define MAXBUF 80

void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);

int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	double error_rate= atof(argv[1]);
	sendErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);

	socketNum = setupUdpClientToServer(&server, argv[2], portNumber);
	
	talkToServer(socketNum, &server);
	
	close(socketNum);

	return 0;
}

void talkToServer(int socketNum, struct sockaddr_in6 * server)
{
	int serverAddrLen = sizeof(struct sockaddr_in6);
	char * ipString = NULL;
	int dataLen = 0;
	int pdu_H_len = 7;
	int pduLen = MAXBUF + 1 + pdu_H_len;
	char buffer[MAXBUF + 1];
	uint8_t pduBuffer[pduLen];

	buffer[0] = '\0';
	uint32_t seq_num = 1;
	uint8_t flag = 23;
	while (buffer[0] != '.')
	{
		dataLen = readFromStdin(buffer);
		createPDU(pduBuffer, seq_num, flag, (uint8_t *)buffer, dataLen);
		seq_num++;
		printf("Creating pdu %s with len %d\n", pduBuffer, dataLen + pdu_H_len);
		printPDU(pduBuffer, pduLen);
		printf("Sending: %s with len: %d\n", buffer, dataLen + pdu_H_len);

		safeSendto(socketNum, pduBuffer, dataLen + pdu_H_len, 0, (struct sockaddr *)server, serverAddrLen);

		safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		
		// print out bytes received
		ipString = ipAddressToString(server);
		printf("Server with ip: %s and port %d said it received %s\n", ipString, ntohs(server->sin6_port), buffer);
	      
	}
}

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

int checkArgs(int argc, char * argv[])
{

    int portNumber = 0;
	float error_rate = atof(argv[1]);
	/* check command line arguments  */
	
	if (argc != 4)
	{
		printf("usage: %s error_rate host-name port-number \n", argv[0]);
		exit(1);
	}
	if (error_rate < 0 || error_rate > 1)
	{
		fprintf(stderr, "Error rate must be >=0 and <=1\n");
		exit(-1);
	}
	portNumber = atoi(argv[3]);
		
	return portNumber;
}





