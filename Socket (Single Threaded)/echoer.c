#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUFFER_SIZE (1024)
#define SERVER_PORT (65530) // custom port number
#define MAX_PENDING_QUEUE_SISZE ((int)100) 

// maximum length to which the queue of pending connections for sockfd may grow
// if excedded, client get "ECONNREFUSED"  error

// broken TCP prevention
int broken = 0;

char nodeType[MAX_BUFFER_SIZE];
char buff[MAX_BUFFER_SIZE];

void usage();
void handler(int);

int main(int argc, char const *argv[])
{

	int sock, cfd;
	struct sockaddr_in serverAddress, clientAddress; // address of INET socket

	signal(SIGPIPE, handler); // to prevent broken TCP connection, set signal handler

	usage();
	if(strcmp(nodeType + 1, "server") == 0){

		// server side
		printf("SERVER executed\n");

		sock = socket (AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			perror ("socket");
			exit(EXIT_FAILURE);
		}

		printf("socket created with fd : %d\n",sock);

		// server side Adress setting
		memset(&serverAddress, 0, sizeof(serverAddress));

	   	serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(SERVER_PORT);
	  	serverAddress.sin_addr.s_addr = htonl (INADDR_ANY);

		// casting INET specific server-side socket address into general sock addr
		if(bind(sock, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0){
			perror("bind");
			exit(EXIT_FAILURE);
		}

		printf("now binded\n");

	   	if (listen(sock, MAX_PENDING_QUEUE_SISZE) < 0){
			perror("listen");
			exit(EXIT_FAILURE);
	   	}

		printf("now listening\n");

		socklen_t clientSideAddressSize = sizeof(clientAddress);

	   	if((cfd = accept(sock, (struct sockaddr *) &clientAddress, &clientSideAddressSize)) < 0){
	   		perror("accept");
	   		exit(EXIT_FAILURE);
	   	}

		printf("now accepted\n");

	   	// once connected, echo buffers
       	while(1){
       		if(broken){
       			printf("TCP broken detected.");
       			exit(EXIT_SUCCESS);
       		}
			printf("in while loop\n");
       		recv(cfd, buff, MAX_BUFFER_SIZE, 0); // -1 on error <- because it's system call
       		printf("received something : %s\n", buff);
       	}

	}else{
		printf("CLIENT executed\n");

		sock = socket (AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			perror ("socket");
			exit(EXIT_FAILURE);
		}

		printf("socket created with fd : %d\n",sock);

		// server side Address setting
		memset(&serverAddress, 0, sizeof(serverAddress));
       	serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(SERVER_PORT);
	  	serverAddress.sin_addr.s_addr = htonl (INADDR_ANY);

    	int serverAddressStructSize = sizeof(serverAddress);
		if(connect(sock, (struct sockaddr*) &serverAddress , serverAddressStructSize) < 0){
			perror("connect");
			exit(EXIT_FAILURE);
		}

		printf("now connected\n");

		// once conencted, get line to echo
		while(1){
       		if(broken){
       			printf("TCP broken detected.");
       			exit(EXIT_SUCCESS);
       		}
       		
			printf("in while loop\n");
			printf("input Message to Send : "); scanf(" %s",buff +1);

			// echo back
			send(sock, buff+ 1, strlen(buff +1), 0);
		}

	}
	return 0;
}

void handler(int s) {
	printf("Caught SIGPIPE\n");
	broken = 1;
}

void usage(){
	printf("please select node kind\n");
	printf("client is default on wrong/any input\n");
	printf("type server or client : ");
	scanf(" %s",nodeType +1);
	printf("---------------------------------\n");
}