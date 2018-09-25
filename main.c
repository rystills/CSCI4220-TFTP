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

/**
child handler (written in lab) 
@param signo: child signal
**/
void sig_child(int signo) {
	pid_t pid;
	int stat;
	while( (pid = waitpid(-1,&stat,WNOHANG)) >0) printf("Parent sees CHILD PID %d has terminated.\n",pid);	
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

int main(int argc, char **argv) { 
	int sockfd;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t s = sizeof(servaddr);
	char buffer[MAXLINE];
	
	//init socket 
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		exitError("unable to create socket"); 
	
	//init server and client sockets
	memset(&servaddr, 0, s);
	memset(&cliaddr, 0, sizeof cliaddr);

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

	//main server loop
	//temp value; replace this bool with forking later
	bool midRequest = false;
	int lastWrittenBlock = 0;
	char fileName[MAXLINE];
	for (;;) {
		socklen_t len = sizeof (struct sockaddr_in);
		printf("len: %d\n", len);
		socklen_t n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &cliaddr, &len);
		printf("len: %d\n", len);
		printf("Client sent this message: ");
		printPacket(buffer, n);
		//write filename to variable
		if (!midRequest) {
			strcpy(fileName, buffer+2);
		}
		printf("filename: %s\n",fileName);

		if (buffer[1] == 2) {
			//write request
			if (!midRequest) {
				//send back initial ACK
				char initialAck[4] = {0,4,0,0};
				printf("len: %d\n", len);
				if (sendto(sockfd, initialAck, 4, 0, (const struct sockaddr *) &cliaddr, len) == -1)
					printf("Error sending: %s\n", strerror(errno));
			}
			else {
				//write data
				//make sure block # is correct
				if (buffer[3] == lastWrittenBlock + 1) {
					//we haven't written yet
				}
				else {
					//we've already written this block; error?
				}
			}
		}
		else if (buffer[1] == 1) {
			buffer[1] = 4;
			buffer[2] = 0;
			buffer[3] = 1;
			buffer[4] = '\0';
			//sendto(sockfd,(;
			//read request
		}
		else {
			//we should not recieve anything else
		}
		midRequest = true;
	}

	return 0; 
}