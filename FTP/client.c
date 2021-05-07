#define _POSIX_SOURCE
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#undef _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define SERVER_IP_ADDRESS (192.168.0.1)
#define SERVER_PORT (12345) // custom port number
#define MAX_BUFFER_SIZE (1024)

void handlerOnConnectionBroken(int s);
void setConnectionBrokenHandler();
void handle_error(char*);

// Message Queue
typedef struct Message{
	int userId;
	char message[MAX_BUFFER_SIZE];
}Message;

int main()
{

	setConnectionBrokenHandler();

	int cfd;
	struct sockaddr_in addr;
	socklen_t addr_size;

	cfd = socket(AF_INET, SOCK_STREAM, 0);
	if (cfd == -1)
        handle_error("socket");

	// server side Address setting
	memset(&addr, 0, sizeof(addr));
   	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
  	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	
   	addr_size = sizeof(struct sockaddr_in);
	if(connect(cfd, (struct sockaddr*) &addr , addr_size) == -1)
		handle_error("connect");

	printf("Connected to Server\n");
	return 0;
}

void handlerOnConnectionBroken(int s) {
	handle_error("[SIGPIPE] connection broken");
}

void setConnectionBrokenHandler(){
	signal(SIGPIPE, handlerOnConnectionBroken); // to prevent broken TCP connection, set signal handler
}

void handle_error(char* message){
	perror(message);
	exit(EXIT_FAILURE);
}
