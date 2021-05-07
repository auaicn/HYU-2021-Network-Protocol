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

#define SERVER_PORT (12345) // custom port number
#define MAX_BUFFER_SIZE (1024)
#define MAX_NUM_CLIENT (1000) // no matters

// Message Queue
typedef struct Message{
	int userId;
	char message[MAX_BUFFER_SIZE];
}Message;

void handlerOnConnectionBroken(int s);
void setConnectionBrokenHandler();
void handle_error(char*);

char *cwd;

int main()
{

	setConnectionBrokenHandler();

	int sfd, cfd;
	struct sockaddr_in my_addr, peer_addr;
	socklen_t peer_addr_size;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
        handle_error("socket");

	memset(&my_addr, 0, sizeof(struct sockaddr_in));

   	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(SERVER_PORT);
  	my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htonl 인듯?
  	
	// casting INET specific server-side sock address into general sock addr
	if(bind(sfd, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1)
		handle_error("bind");

	if(listen(sfd, MAX_NUM_CLIENT) == -1)
		handle_error("listen");

	printf("server is now listening on PORT: %d\n",SERVER_PORT);

   	peer_addr_size = sizeof(struct sockaddr_in);
    cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (cfd == -1)
        handle_error("accept");

	printf("server connected\n");

	while(1){
		
	}

	close(cfd);
	return 0;
}

void getDEntries(char *path)
{
	DIR *dir;
	struct dirent *entry;

	if ((dir = opendir(path)) == NULL)
		perror("opendir() error");
	else
	{
		puts("contents of root:");
		while ((entry = readdir(dir)) != NULL)
			printf("  %s\n", entry->d_name);
		closedir(dir);
	}
}

/// returns null if not existing
char *setCWDtoPWD(char *_cwd)
{
	printf("%lu", sizeof(_cwd));
	if (getcwd(_cwd, sizeof(_cwd)) != NULL)
	{
		printf("Current working dir: %s\n", _cwd);
		return _cwd;
	}

	perror("getcwd() error");
	return NULL;
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
