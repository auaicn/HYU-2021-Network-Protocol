#define _POSIX_SOURCE
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

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
#include <ctype.h>

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>

#define SUBSCRIBER_PORT (1000)
#define MAX_TOPIC_LENGTH (1024)
#define MAX_TOPIC_LENGTH (1024)
#define MAX_FILE_LENGTH (16384)

typedef struct Message
{
  char topic[MAX_TOPIC_LENGTH];
  char contents[MAX_FILE_LENGTH];
} Message;

typedef struct Port
{
  int receiver;
  int sender;
} Port;

bool receiver_exists = false;
Message *newMessage();
void receiver_init(int *);
void *receiver_routine(void *);
void main_thread_as_topic_sender(int);

pthread_t receiver_thread; // Message queue 에서 활용하기 위함.

int main(int argc, const char *argv[])
{
  // senders
  int sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in broker_address;

  // server side Address setting
  memset(&broker_address, 0, sizeof(broker_address));
  broker_address.sin_family = AF_INET;
  broker_address.sin_port = htons(SUBSCRIBER_PORT);
  broker_address.sin_addr.s_addr = htons(INADDR_ANY);
  // port 를 받아 receiver thread 를 생성한다.

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("sock");
    exit(EXIT_FAILURE);
  }

  printf("[main] socket created\n");

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return 0;
  }

  printf("[main] socket option set to SO_REUSEADDR\n");

  if (connect(sock, (struct sockaddr *)&broker_address, sizeof(broker_address)) < 0)
  {
    perror("connect");
    exit(EXIT_FAILURE);
  }

  printf("[main] connected to broker\n");

  Port port;

  // receive port
  if (recv(sock, &port, sizeof(Port), 0) < 0)
  {
    perror("recv");
    exit(EXIT_FAILURE);
  }

  // 서버측 sender 의 port 가 subscriber receiver 의 port 번호가 되고
  // 서버측 receiver 의 port 가 subscriber sender (main thread) 의 port 번호가 된다.
  int receiver_port = port.sender; // 20001
  int sender_port = port.receiver; // 20000
  printf("port %d and %d is given from broker\n", receiver_port, sender_port);

  if (!receiver_exists)
    receiver_init(&receiver_port);

  close(sock);

  main_thread_as_topic_sender(sender_port);

  return 0;
}

void main_thread_as_topic_sender(int port)
{

  printf("[main thread] with port %d\n", port);
  // main thread 는 sender_port 를 가지고, socket 을 다시 열고,
  // reuse broker address
  // server side Address setting
  int sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in broker_address;

  // server side Address setting
  memset(&broker_address, 0, sizeof(broker_address));
  broker_address.sin_family = AF_INET;
  broker_address.sin_port = htons(port);
  broker_address.sin_addr.s_addr = htons(INADDR_ANY);
  // port 를 받아 receiver thread 를 생성한다.

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("sock");
    exit(EXIT_FAILURE);
  }

  printf("[main thread] socket created\n");

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return;
  }

  printf("[main thread] socket option changed to SQ_REUSEADDR\n");

  sleep(1);
  if (connect(sock, (struct sockaddr *)&broker_address, sizeof(broker_address)) < 0)
  {
    perror("[main, sender] connect");
    exit(EXIT_FAILURE);
  }

  printf("[main thread] connected to broker\n");

  char topic_to_subscribe[MAX_TOPIC_LENGTH];
  while (true)
  {
    memset(topic_to_subscribe, '\0', MAX_TOPIC_LENGTH);

    printf("enter topics to subscribe / unsubscribe :");
    scanf(" %s", topic_to_subscribe);

    int bytesSent = send(sock, topic_to_subscribe, sizeof(topic_to_subscribe), 0);
    printf("%d bytes sent\n", bytesSent);
  }
}

void receiver_init(int *port)
{
  // printf("receiver_init : %d\n", *port);
  printf("initializing receiver\n");

  if (pthread_create(&receiver_thread, NULL, receiver_routine, (void *)port) < 0)
  {
    perror("failed to create [sender] thread\n");
    exit(0);
  }
  receiver_exists = true;
}

// received then print
void *receiver_routine(void *port)
{

  int *port_number = (int *)port;
  printf("[receiver] %d\n", *port_number);

  int sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in sender;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("sock");
    exit(EXIT_FAILURE);
  }

  printf("[receiver] socket created\n");

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return NULL;
  }

  printf("[receiver] socket option changed to SO_REUSEADDR\n");

  memset(&sender, 0, sizeof(sender));
  sender.sin_family = AF_INET;
  sender.sin_port = htons(*port_number);
  sender.sin_addr.s_addr = htons(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htons 인듯?

  if (bind(sock, (struct sockaddr *)&sender, sizeof(sender)) < 0)
  {
    perror("[receiver] bind");
    exit(EXIT_FAILURE);
  }

  printf("[receiver] now binded\n");

  if (listen(sock, 1) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("[receiver] now listening\n");

  int publisher_sock_fd;
  if ((publisher_sock_fd = accept(sock, NULL, NULL)) < 0)
  {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  while (true)
  {
    Message *message = newMessage();

    // receive message
    if (recv(publisher_sock_fd, message, sizeof(Message), 0) < 0)
    {
      perror("recv");
      exit(EXIT_FAILURE);
    }

    printf("\nnew message\n\n");
    printf("Topic : %s\n\n%s\n", message->topic, message->contents);
  }
}

void subscribe(char *topic)
{
}

void unsubscribe(char *topic)
{
}

Message *newMessage()
{
  Message *message = (Message *)malloc(sizeof(Message));
  memset(message->topic, '\0', MAX_TOPIC_LENGTH);
  memset(message->contents, '\0', MAX_FILE_LENGTH);
  return message;
}