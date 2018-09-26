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
#include <inttypes.h>

#define MAXLINE 1024

//globals (for signal use)
int numResends = 0;
char lastMessage[MAXLINE];
int lastMessageLen;
struct sockaddr_in cliaddr;
int sockfd;
//global io error
const char* errorMessage = "\0\5\0\0IO error detected; aborting";

void sig_child(int signo) {
	pid_t pid;
	int stat;

	while( (pid = waitpid(-1,&stat,WNOHANG)) >0) {
		printf("Parent sees CHILD PID %d has terminated with status %d.\n",pid,stat);	
	}
	//printf("Parent spawned child PID %d\n",pid);
	//pid = wait(&stat);
	//printf("child %d terminated\n", pid);
}

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
		printf("Timed out, resends done\n");
		fflush(stdout);
		exit(EXIT_FAILURE);
	}
	printf("time out %d, resending message\n",numResends);
	fflush(stdout);
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

unsigned int packetBlockNumber(const char* buffer)
{
	return *((unsigned char*) buffer+3) | *((unsigned char*) buffer+2)<<8;
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
			printf("DATA|%d|%.*s\n", packetBlockNumber(buffer), (int) len-4, buffer+4);
			break;
		case 4:
			printf("ACK|%u\n", packetBlockNumber(buffer));
			break;
		case 5:
			printf("ERROR|%d|%s\n", packetBlockNumber(buffer), buffer+4);
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

void sendAck(unsigned int blockdigit1, unsigned int blockdigit2)
{
	lastMessageLen = 4;
	//printf("Sending ACK %u\n", blockNumber);
	char ack[4] = {0,4,blockdigit1,blockdigit2};
	if (sendto(sockfd, ack, 4, 0, (const struct sockaddr *) &cliaddr, sizeof (struct sockaddr_in)) == -1)
		printf("Error sending: %s\n", strerror(errno));
	strncpy(lastMessage,ack,4);
	//reset the timeout alarm cooldown for 1 second
	alarm(1);
}

void makeData(char* packet, unsigned int blockNumber)
{
	packet[0] = 0;
	packet[1] = 3;
	packet[2] = (char)(blockNumber>>8);
	packet[3] = (char)blockNumber;
}

void handleWrite(const char* fileName)
{
	//get a pointer to the file, creating it if it doesn't exist
	FILE *fp = fopen(fileName, "wb");
	if (fp == NULL) {
		sendPacket(errorMessage,31);
		exit(EXIT_FAILURE);
	}

	signal(SIGALRM, sig_timeout);
	char buffer[MAXLINE];
	sockfd = initSocket();

	//send back initial ACK
	sendAck(0,0);

	unsigned int currBlock = 1;
	socklen_t n;
	do
	{
		socklen_t len;
		n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		printf("n: %d\n",n);
		fflush(stdout);
		printPacket(buffer, n);
		sendAck(buffer[2],buffer[3]);
		printf("current ack is %u\n",packetBlockNumber(buffer));
		if (packetBlockNumber(buffer) == currBlock)
		{
			printf("writing: %s\n",buffer+4);
			//write the contents to the file
			fwrite(buffer+4, sizeof (char), n-4, fp);
			++currBlock;
			printf("curBlock is %d\n",currBlock);
			if (currBlock == 148) {
				printf("ack equivalent of 148 is %" PRIu16 "\n",buffer[3]);
			}
		}
	} while (n == 516);
	//close socket and file pointers
	close(sockfd);
	fclose(fp);

	//turn off the timeout alarm and exit
	alarm(0);
	exit (0);
}

void receiveAck(unsigned int blockNumber)
{
	socklen_t len = sizeof cliaddr;
	char data[4];
	do
	{
		alarm(1);
		recvfrom(sockfd, data, 4, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		printf("blockNumber: %u\t", blockNumber);
		printPacket(data, 4);
	}
	while (data[1] != 4 || packetBlockNumber(data) != blockNumber);
	alarm(0);
}

void handleRead(const char* fileName)
{
	signal(SIGALRM, sig_timeout);
	char buffer[MAXLINE];
	sockfd = initSocket();

	FILE* file = fopen(fileName, "rb");
	if (file == NULL) {
		sendPacket(errorMessage,31);
		exit(EXIT_FAILURE);
	}

	size_t numRead = 512;
	for (unsigned int blockNumber = 1; numRead == 512; receiveAck(blockNumber++))
	{
		// Read up to 512 bytes from file and then send to client
		char data[512+4];
		makeData(data, blockNumber);
		numRead = fread(data+4, sizeof(char), 512, file);
		memcpy(lastMessage, data, numRead+4);
		lastMessageLen = numRead+4;
		sendPacket(data, numRead+4);
	}

	close(sockfd);
	//turn off the timeout alarm
	alarm(0);
	exit(0);
}

int main(int argc, char **argv) { 
	sockfd = initSocket();
	char buffer[MAXLINE];
	signal(SIGCHLD, sig_child);
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