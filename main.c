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

int main(int argc, char **argv) { 
    int sockfd; 
    char buffer[MAXLINE]; 
    struct sockaddr_in servaddr, cliaddr; 
      
    //init socket 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) exitError("unable to create socket"); 
    
    //init server and client sockets
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    //assign server properties 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = 0; 
      
    //bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  sizeof(servaddr)) < 0 ) exitError("unable to bind socket");

    //check/get sin_port
    socklen_t s = sizeof(servaddr);
    getsockname(sockfd,(struct sockaddr *)&servaddr,&s);
	unsigned int port = ntohs(servaddr.sin_port);
    printf("port is: %d\n",port);
	fflush(stdout);
    
    //handle messages
    /*socklen_t len, n; 
    n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len); 
    buffer[n] = '\0'; 
    printf("Client : %s\n", buffer); 
    sendto(sockfd, (const char *)hello, strlen(hello), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len); 
    printf("Hello message sent.\n");*/
      
	//main server loop
	for (;;) {
		socklen_t len, n;
		 n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &cliaddr, &len); 
		 buffer[n] = '\0'; 
		 printf("Client sent this message: ");
		 for (int i = 0; i < n; ++i) {
		 	printf("%d,",buffer[i]);
		 } 
		 printf("\n");
		 printf("message size is: %d\n",n);
		 
		 if (buffer[1] == 2) {
		 	//write request
		 	buffer[1] = 4;
		 	buffer[2] = 0;
		 	buffer[3] = 0;
		 	buffer[4] = '\n';
		 	sendto(sockfd, buffer, 5, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len); 
		 }
		 else if (buffer[1] == 1) {
		 	buffer[1] = 4;
		 	buffer[2] = 0;
		 	buffer[3] = 1;
		 	buffer[4] = '\n';
		 	//sendto(sockfd,(;
		 	//read request
		 }
		 else {
		 	//we should not receieve anything else yet
		 }
	}

    return 0; 
}