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

// Messages
Message *messages;

int front = -1;
int rear = -1;

int isFull();
int isEmpty();
int enqueue(Message message);
Message *dequeue();

// senders
int sock;
int senderThreadID;
pthread_t senderThread; // Message queue 에서 활용하기 위함.
struct sockaddr_in serverAddress;

// receirvers
int socketOfClients[10];
int receiverThreadsID[MAX_NUM_CLIENT];
pthread_t receiverThreads[MAX_NUM_CLIENT];
struct sockaddr_in clientAddress[MAX_NUM_CLIENT]; // address of INET sock

// workloads
void *senderWorkload();
void *receiverWorkload(void *);
void handlerOnConnectionBroken(int s);

// Concurrency Control
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char const *argv[])
{

	messages = (Message *)malloc(sizeof(Message) * MAX_MESSAGE_QUEUE_SIZE);

	signal(SIGPIPE, handlerOnConnectionBroken); // to prevent broken TCP connection, set signal handler

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("sock");
		exit(EXIT_FAILURE);
	}

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htonl 인듯?

	printf("%lu", sizeof(u_long));
	printf("%lu", sizeof(unsigned long));
	printf("%lu", sizeof(unsigned int));
	printf("%u\n", INADDR_ANY);
	// printf("%ul\n",INADDR_ANY);

	// casting INET specific server-side sock address into general sock addr
	if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	// printf("now binded\n");

	if (listen(sock, MAX_NUM_CLIENT) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	// printf("now listening\n");

	// SENDER THREADS
	senderThreadID = pthread_create(&senderThread, NULL, senderWorkload, 0);

	if (senderThreadID < 0)
	{
		printf("Failed to create ThreadID Create\n");
		exit(EXIT_FAILURE);
	}

	// RECEIVER THREADS
	// Accepted then create
	int threadCount = 0;
	while (1)
	{
		if (threadCount < MAX_NUM_CLIENT)
		{
			// printf("enough\n");

			if ((socketOfClients[threadCount] = accept(sock, NULL, NULL)) < 0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			// make receiver thread
			int userId = threadCount; // passed to thread as argument
																// printf("userId is %d\n",userId);

			if ((receiverThreadsID[threadCount] =
							 pthread_create(&receiverThreads[threadCount],
															NULL, receiverWorkload, (void *)&userId)) < 0)
			{
				perror("failed to create [sender] thread");
				exit(0);
			}

			threadCount++;
			// printf("now threadCount is %d\n",threadCount);
		}
		else
		{
			printf("Chat Room Already Occuplied by Full Members\n");
			continue; // escape
		}
	}

	return 0;
}

void *senderWorkload()
{

	// printf("sender workload executed\n");
	Message *front;
	printf("Sender Thread Started\n");

	while (1)
	{
		if ((front = dequeue()) != NULL)
		{

			for (int id = 0; id < MAX_NUM_CLIENT; id++)
			{
				send(socketOfClients[id], (char *)front, sizeof(Message), 0);
			}
		}
	}
	return (void *)0;
}

void *receiverWorkload(void *data)
{

	int *tid = (int *)data;
	printf("receiver thread created with tid [%d]\n", *tid);
	printf("socket : %d\n", socketOfClients[*tid]);

	Message receivedMessage;
	memset(&receivedMessage, '\0', sizeof(Message));

	while (recv(socketOfClients[*tid], (char *)&receivedMessage, sizeof(receivedMessage), 0) > 0)
	{
		printf("tid is %d\n", *tid);
		receivedMessage.userId = *tid;
		if (enqueue(receivedMessage) == -1)
		{
			printf("Message Buffer Full\n");
		}
	}

	printf("receiver destroyed\n");
	return (void *)0;
}

void handlerOnConnectionBroken(int s)
{
	printf("Caught SIGPIPE\n");
	printf("서버와의 연결이 종료되었습니다\n");
	exit(EXIT_FAILURE);
}

// Message Queue Interface
// front to Rear 은 가득 차 있다고 가정한다.
int isFull()
{
	// circular queue 한바퀴 돈 경
	if (front == rear + 1)
		return 1;
	else
		return 0;
}

int isEmpty()
{
	if (rear == front)
		return 1;
	else
		return 0;
}

int enqueue(Message mes)
{

	if (isFull())
		return -1;

	// critical section
	pthread_mutex_lock(&mutex);
	if (front == -1)
		front = 0;

	rear = (rear + 1) % MAX_MESSAGE_QUEUE_SIZE;
	messages[rear].userId = mes.userId;
	strcpy(messages[rear].message, mes.message);

	printf("enquede, rear[%d] id[%d] : %s\n", rear, messages[rear].userId, messages[rear].message);
	pthread_mutex_unlock(&mutex);
	return 0; // on success
}

Message *dequeue()
{

	Message *mes;

	if (isEmpty())
		return NULL;

	pthread_mutex_lock(&mutex);
	mes = &messages[front]; // extract front

	// printf("extracted message : %s\n", mes -> message);
	front = (front + 1) % MAX_MESSAGE_QUEUE_SIZE; // pop front & adjust

	pthread_mutex_unlock(&mutex);
	return mes;
}
