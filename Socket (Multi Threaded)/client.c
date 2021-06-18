#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define MAX_BUFFER_SIZE (1024)
#define MAX_NUM_CLIENT (10)
#define SERVER_PORT (12345) // custom port number
#define MAX_MESSAGE_QUEUE_SIZE (100)

// Message Queue
typedef struct Message
{
	int userId;
	char message[MAX_BUFFER_SIZE];
} Message;

int broken = 0;
int sock;

int senderSocketFd;
struct sockaddr_in serverAddress, clientAddress; // address of INET socket

void *workload(void *data);
void handlerOnConnectionBroken(int s);

Message messageFromServer;

int main(int argc, char const *argv[])
{

	signal(SIGPIPE, handlerOnConnectionBroken); // to prevent broken TCP connection, set signal handler

	// multi - threaded implementation
	pthread_t receiverThread;
	int receiverThreadID, status;

	// server side Address setting
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
	{
		perror("connect");
		exit(EXIT_FAILURE);
	}

	printf("Connected to Server\n");

	if (pthread_create(&receiverThread, NULL, workload, 0) < 0)
	{
		perror("failed to create [sender] thread");
		exit(0);
	}

	// once conencted, get line to echo
	Message messageToSend;
	int bytesSent;
	while (1)
	{

		memset(&messageToSend, 0, sizeof(messageToSend));
		// printf("input Message to Send : ");
		printf("> ");
		// input Message to Send : ");
		scanf(" %s", messageToSend.message);

		messageToSend.message[strlen(messageToSend.message)] = '\0';
		messageToSend.userId = -1; // 어짜피 알 수 없다.

		// printf("before send\n");

		bytesSent = send(sock, (char *)&messageToSend, sizeof(Message), 0);
		// printf("%d bytes sent\n",bytesSent);
	}

	// wait for receiver thread to end
	if (pthread_join(receiverThread, (void **)&status) < 0)
	{
		perror("joining sender Thread Failed");
		exit(0);
	}

	return 0;
}

void handlerOnConnectionBroken(int s)
{
	printf("Caught SIGPIPE\n");
	printf("서버와의 연결이 종료되었습니다\n");
	exit(EXIT_FAILURE);
}

void *workload(void *data)
{
	// 받은 채팅 내용을 클라이언트 화면에 보여준다
	// once connected, echo buffers
	// printf("workload created\n");
	while (1)
	{
		if (broken)
		{
			printf("TCP broken detected.");
			exit(EXIT_SUCCESS);
		}
		if (recv(sock, (char *)&messageFromServer, sizeof(messageFromServer), 0) < 0)
		{
			handlerOnConnectionBroken(-1);
			return (void *)0;
		}
		printf("User %d: %s\n", messageFromServer.userId, messageFromServer.message);
	}

	return (void *)0;
}
