#include <stdio.h>  
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <setjmp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>

#include "unp.h"

void sig_child(int signo) {
	pid_t pid;
	int stat;

	while( (pid = waitpid(-1,&stat,WNOHANG)) >0) {
		printf("Parent sees CHILD PID %d has terminated.\n",pid);	
	}
}

int main(int argc, char **argv) {
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;
	bind(sockfd, &addr, sizeof(addr));
	printf(getsockname(sockfd,addr));
}