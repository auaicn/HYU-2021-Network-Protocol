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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>

#define PUBLISHER_PORT (1001)
#define MAX_FILE_LENGTH (16384)
#define MAX_FILENAME_LENGTH (1024)
#define MAX_TOPIC_LENGTH (1024)

// Message Queue
typedef struct Message
{ // reusable
  char topic[MAX_TOPIC_LENGTH];
  char contents[MAX_FILE_LENGTH];
} Message;

int senderSocketFd;
struct sockaddr_in serverAddress, clientAddress; // address of INET socket

Message *newMessage();
bool isValidFileName(char *);
void readFromFile(char *, Message *);
int post(Message *);

int main(int argc, const char *argv[])
{
  char fileName[MAX_FILENAME_LENGTH];

  while (true)
  {
    Message *message = newMessage();
    memset(fileName, '\0', MAX_FILENAME_LENGTH);

    // topic
    printf("\ninput topic: ");
    scanf("%s", message->topic);

    // filename
    printf("input fileName: ");
    scanf("%s", fileName);

    // validate file name
    while (!isValidFileName(fileName))
    {
      printf("input fileName: ");
      scanf("%s", fileName);
    }

    readFromFile(fileName, message);

    if (post(message) < 0)
    {
      printf("failed to post\n");
      return 0;
    }
  }
}

int post(Message *message)
{
  int sock;
  int flag = 1;
  int size = sizeof(flag);

  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PUBLISHER_PORT);
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    perror("connect");
    exit(EXIT_FAILURE);
  }

  printf("Connected to Server\n");

  int bytesSent;
  bytesSent = send(sock, message, sizeof(Message), 0);
  printf("%d bytes sent\n", bytesSent);

  close(sock);
  return 0;
}

bool isValidFileName(char *filePath)
{
  FILE *fp;
  if ((fp = fopen(filePath, "r")) == NULL)
  {
    printf("Failed to open file \"%s\"\n", filePath);
    return false;
  }
  return true;
}

void readFromFile(char *filePath, Message *message)
{
  FILE *fp;
  if ((fp = fopen(filePath, "r")) == NULL)
  {
    printf("Failed to open file \"%s\"\n", filePath);
    return;
  }

  fread(message->contents, 1, MAX_FILE_LENGTH, (FILE *)fp);
  printf("read file\n");
  printf("%s\n", message->contents);
}

Message *newMessage()
{
  Message *message = (Message *)malloc(sizeof(Message));
  memset(message->topic, '\0', MAX_TOPIC_LENGTH);
  memset(message->contents, '\0', MAX_FILE_LENGTH);
  return message;
}