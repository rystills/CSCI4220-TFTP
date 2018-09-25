#include <stdio.h>  
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>
#include <stdbool.h>

#define MAXLINE 1024

//globals (for signal use)
int numResends = 0;
char lastMessage[MAXLINE];
int lastMessageLen;
struct sockaddr_in cliaddr;
int sockfd;

void sendPacket(const char* packet, int len)
{
	lastMessageLen = len;
	if (sendto(sockfd, packet, len, 0, (const struct sockaddr *) &cliaddr, sizeof (struct sockaddr_in)) == -1)
		printf("Error sending: %s\n", strerror(errno));
	//reset the timeout alarm cooldown for 1 second
	alarm(1);
}

void sig_timeout(int signo) {
	if (++numResends == 10) {
		//we've timed out! cleanup and leave
		//close(3);
		exit(EXIT_FAILURE);
	}
	//resend the last message (this also refreshes the alarm cooldown)
	sendPacket(lastMessage, lastMessageLen);
}

/**
exit with an error message
@param str: the error string to display
**/
void exitError(char* str) {
	perror(str); 
	exit(EXIT_FAILURE); 
}

/**
print the contents of the buffer containing packet info
@param buffer: the paket buffer to print
@param len: the buffer size
**/
void printPacket(const char* buffer, size_t len)
{
	switch (buffer[1])
	{
		case 1:
			printf("RRQ|%s|%s\n", buffer+2, strchr(buffer+2, 0)+1);
			break;
		case 2:
			printf("WRQ|%s|%s\n", buffer+2, strchr(buffer+2, 0)+1);
			break;
		case 3:
			printf("DATA|%d|%.*s\n", buffer[3], (int) len-4, buffer+4);
			break;
		case 4:
			printf("ACK|%d\n", buffer[3]);
			break;
		case 5:
			printf("ERROR|%d|%s\n", buffer[3], buffer+4);
			break;
		default:
			printf("Invalid packet\n");
	}
}

int initSocket()
{
	struct sockaddr_in servaddr;

	socklen_t s = sizeof servaddr;
	//init socket
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		exitError("unable to create socket"); 
	
	//init server and client sockets
	memset(&servaddr, 0, s);

	//assign server properties
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = 0;
	
	//bind the socket with the server address
	if (bind(sockfd, (const struct sockaddr *) &servaddr, s) < 0)
		exitError("unable to bind socket");

	//check/get sin_port
	getsockname(sockfd, (struct sockaddr *) &servaddr,&s);
	printf("port is: %d\n", ntohs(servaddr.sin_port));
	fflush(stdout);

	return sockfd;
}

void sendAck(int blockNumber)
{
	lastMessageLen = 4;
	printf("Sending ACK %d\n", blockNumber);
	char ack[4] = {0,4,0,blockNumber};
	if (sendto(sockfd, ack, 4, 0, (const struct sockaddr *) &cliaddr, sizeof (struct sockaddr_in)) == -1)
		printf("Error sending: %s\n", strerror(errno));
	strncpy(lastMessage,ack,4);
	//reset the timeout alarm cooldown for 1 second
	alarm(1);
}

void makeData(char* packet, int blockNumber)
{
	packet[0] = 0;
	packet[1] = 3;
	packet[2] = 0;
	packet[3] = blockNumber;
}

void handleWrite(const char* fileName)
{
	char buffer[MAXLINE];
	sockfd = initSocket();

	//send back initial ACK
	sendAck(0);

	int currBlock = 1;
	socklen_t n;
	do
	{
		socklen_t len;
		n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		printPacket(buffer, n);
		sendAck(buffer[3]);
		if (buffer[3] == currBlock)
		{
			printf("writing: %s\n",buffer+4);
			++currBlock;
		}
	} while (n < 512);
	close(sockfd);
	//turn off the timeout alarm
	alarm(0);
	exit (0);
}

void receiveAck(int blockNumber)
{
	socklen_t len = sizeof cliaddr;
	char data[4];
	do
	{
		alarm(1);
		recvfrom(sockfd, data, 4, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
	}
	while (data[1] != 4 || data[3] != blockNumber);
}

void handleRead(const char* fileName)
{
	char buffer[MAXLINE];
	sockfd = initSocket();

	FILE* file = fopen(fileName, "rb");
	size_t numRead = 512;
	for (int blockNumber = 1; numRead == 512; receiveAck(blockNumber++))
	{
		// Read up to 512 bytes from file and then send to client
		char data[512+4];
		makeData(data, blockNumber);
		numRead = fread(data+4, sizeof(char), 512, file);
		memcpy(lastMessage, data, numRead+4);
		sendPacket(data, numRead+4);
	}

	close(sockfd);
	//turn off the timeout alarm
	alarm(0);
	exit(0);
}

int main(int argc, char **argv) { 
	signal(SIGALRM, sig_timeout);
	sockfd = initSocket();
	char buffer[MAXLINE];
	
	while (true)
	{
		socklen_t len = sizeof cliaddr;
		socklen_t n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		printPacket(buffer, n);
		if (buffer[1] == 2 && fork() == 0)
			handleWrite(buffer+2);
		if (buffer[1] == 1 && fork() == 0)
			handleRead(buffer+2);
	}
	return 0;
}