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

using namespace std;

#define SUBSCRIBER_PORT (1000)
#define PUBLISHER_PORT (1001)
#define MAX_NUM_PUBLISHER (100)
#define MAX_FILE_LENGTH (16384)
#define MAX_TOPIC_LENGTH (1024)
#define MAX_NUM_SUBSCRIBER (1024)

typedef struct Message
{
  char topic[MAX_TOPIC_LENGTH];
  char contents[MAX_FILE_LENGTH];
} Message;

typedef struct Subscriber
{
  vector<pair<string, int>> subscribed_topics;
  int sock;
} Subscriber;

typedef struct Port
{
  int receiver; // 20000
  int sender;   // 20001
} Port;

typedef struct User
{
  int port;
  int id;
} User;

map<string, vector<string>> topics;

void ps_init();
void publisher_init();
void subscriber_init();
Message *newMessage();
void list_all_topics_and_contents();
void *handle_sender(void *);
void *handle_receiver(void *);
void *handle_publisher(void *);
void *handle_subscriber(void *);
void make_new_sender_thread(int);
int register_message(Message *);
int distribute_message(string);

pthread_t p_thread, s_thread;
pthread_cond_t p_thread_cond = PTHREAD_COND_INITIALIZER;

int port_for_each_subscriber = 20000; // mutex_message 와 같이 사용할 것

pthread_mutex_t mutex_message = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sender = PTHREAD_MUTEX_INITIALIZER;

map<int, map<string, pair<bool, int>>> user_informations;

int main(int argc, const char *argv[])
{
  ps_init();

  int p_status, s_status;
  pthread_join(p_thread, (void **)&p_status);
  pthread_join(s_thread, (void **)&s_status);

  return 0;
}

void ps_init()
{
  publisher_init();
  subscriber_init();
}

void publisher_init()
{
  int a;

  pthread_create(&p_thread, NULL, handle_publisher, (void *)&a);
}

void subscriber_init()
{
  int a;

  pthread_create(&p_thread, NULL, handle_subscriber, (void *)&a);
}

void *handle_publisher(void *data)
{

  int sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in broker_address;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("sock");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return NULL;
  }

  memset(&broker_address, 0, sizeof(broker_address));
  broker_address.sin_family = AF_INET;
  broker_address.sin_port = htons(PUBLISHER_PORT);
  broker_address.sin_addr.s_addr = htons(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htons 인듯?

  // casting INET specific server-side sock address into general sock addr
  if (::bind(sock, (struct sockaddr *)&broker_address, sizeof(broker_address)) < 0)
  {
    perror("[publisher handler] bind");
    exit(EXIT_FAILURE);
  }

  printf("now binded\n");

  if (listen(sock, MAX_NUM_PUBLISHER) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("now listening\n");

  while (true)
  {
    int publisher_sock;
    if ((publisher_sock = accept(sock, NULL, NULL)) < 0)
    {
      /*
      extracts the first
      connection request on the queue of pending connections for the
      listening socket
      */

      /*
      If this happens, then the call will block waiting for
      the next connection to arrive.
      성능은 느리지만 원하는 시나리오네
      */

      perror("accept");
      exit(EXIT_FAILURE);
    }

    printf("accepted\n");

    Message *message = newMessage();
    if (recv(publisher_sock, message, sizeof(Message), 0) > 0)
    {
      printf("received\n\n");
      printf("topic : %s\n", message->topic);
      printf("contents : %s\n", message->contents);

      // -------------- critical section --------------
      pthread_mutex_lock(&mutex_message);

      if (register_message(message) < 0)
      {
        printf("failed to register topic");
        pthread_mutex_unlock(&mutex_message);
        continue;
      }

      printf("topic registered\n");

      if (distribute_message(message->topic) < 0)
      {
        printf("failed to register topic");
        pthread_mutex_unlock(&mutex_message);
        continue;
      }

      // printf("messages sent to subscribers\n");

      pthread_mutex_unlock(&mutex_message);
      // -------------- critical section --------------
    }
    else
    {
      printf("something went wrong with recv()! %s\n", strerror(errno));
    }
  }

  printf("receiver destroyed\n");
  return (void *)0;
}

// receivers
int subscriber_idx = 0;
int subscriber_fds[10];
int subscriber_thread_id[MAX_NUM_SUBSCRIBER];
pthread_t subscriber_threads[MAX_NUM_SUBSCRIBER];
struct sockaddr_in subscriber_addresses[MAX_NUM_SUBSCRIBER]; // address of INET sock

void *handle_subscriber(void *data)
{
  int sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in broker_address;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("sock");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return NULL;
  }

  memset(&broker_address, 0, sizeof(broker_address));
  broker_address.sin_family = AF_INET;
  broker_address.sin_port = htons(SUBSCRIBER_PORT);
  broker_address.sin_addr.s_addr = htons(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htons 인듯?

  int bind_status = ::bind(sock, (struct sockaddr *)&broker_address, sizeof(broker_address));
  if (bind_status < 0)
  {
    perror("[subscriber handler] bind");
    exit(EXIT_FAILURE);
  }

  printf("now binded\n");

  if (listen(sock, MAX_NUM_SUBSCRIBER) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("now listening\n");

  while (true)
  {
    // sender
    int publisher_sock_fd;
    int subscriber_id;

    if ((publisher_sock_fd = accept(sock, NULL, NULL)) < 0)
    {
      perror("accept");
      exit(EXIT_FAILURE);
    }

    printf("\nsubscriber accepted\n");

    struct Port port;

    // -------------- critical section --------------
    pthread_mutex_lock(&mutex_sender);
    subscriber_id = subscriber_idx++;
    port.receiver = port_for_each_subscriber++;
    port.sender = port_for_each_subscriber++;
    printf("user with id (%d) has given two ports (%d) for sender & (%d) for receiver\n", subscriber_id, port.sender, port.receiver);
    pthread_mutex_unlock(&mutex_sender);
    // -------------- critical section --------------

    int bytesSent;
    pthread_t sender, receiver;
    bytesSent = send(publisher_sock_fd, &port, sizeof(Port), 0);
    printf("%d bytes sent\n", bytesSent); // 4 byte 겠지 머

    User data_sender, data_receiver;

    data_sender.id = subscriber_id;
    data_sender.port = port.sender;
    if (pthread_create(&sender, NULL, handle_sender, (void *)&data_sender) < 0)
    {
      perror("failed to create [sender] thread");
      exit(0);
    }

    data_receiver.id = subscriber_id;
    data_receiver.port = port.receiver;
    if (pthread_create(&receiver, NULL, handle_receiver, (void *)&data_receiver) < 0)
    {
      perror("failed to create [sender] thread");
      exit(0);
    }
  }
}

void *handle_sender(void *data)
{

  User *information = (User *)data;
  int id = information->id;
  int port = information->port;

  int sender_sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in sender_address;

  sender_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sender_sock < 0)
  {
    perror("sender_sock");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(sender_sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return NULL;
  }

  memset(&sender_address, 0, sizeof(sender_address));
  sender_address.sin_family = AF_INET;
  sender_address.sin_port = htons(port);
  sender_address.sin_addr.s_addr = htons(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htons 인듯?

  printf("[sender] port %d\n", port);

  sleep(1);
  if (connect(sender_sock, (struct sockaddr *)&sender_address, sizeof(sender_address)) < 0)
  {
    perror("[sender] connect");
    exit(EXIT_FAILURE);
  }

  printf("[sender] now connected with subscriber\n");

  // 일어나면 갱신하고 잔다
  while (true)
  {

    pthread_mutex_lock(&mutex_sender);
    printf("sender thread gone sleep\n");

    pthread_cond_wait(&p_thread_cond, &mutex_sender);

    printf("sender thread woke up\n");

    cout << "looking user with id [" << id << "]" << endl;
    // 일어나면 락을 받은 상태이다.
    // 현재 연결된 유저가 가지고 있지 않은, 새로운 데이터를 보내준다.
    for (auto topic_information : user_informations[id])
    {
      string topic = topic_information.first;
      bool isSubscribed = topic_information.second.first;
      int isReadUpTo = topic_information.second.second;
      cout << "looking topic [" << topic << "]" << endl;
      if (isSubscribed)
      {
        if (isReadUpTo < topics[topic].size())
        {
          printf("해당 토픽 사이즈 %d\n", isReadUpTo);
          for (int i = isReadUpTo; i < topics[topic].size(); i++)
          {
            Message *message = newMessage();
            strcpy(message->topic, topic.c_str());
            strcpy(message->contents, topics[topic][i].c_str());

            int bytesSent = send(sender_sock, message, sizeof(Message), 0);
            printf("%d bytes sent\n", bytesSent); // 4 byte 겠지 머
          }
          user_informations[id][topic].second = topics[topic].size();
        }
      }
    }
    printf("sender thread releasing lock\n");

    pthread_mutex_unlock(&mutex_sender);
  }
}

void *handle_receiver(void *data)
{
  User *information = (User *)data;
  int id = information->id;
  int port = information->port;

  int sock;
  int flag = 1;
  int size = sizeof(flag);
  struct sockaddr_in sender_address;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror("sock");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, size) == -1)
  {
    fprintf(stderr, "socket 생성 error: %s\n", strerror(errno));
    return NULL;
  }

  memset(&sender_address, 0, sizeof(sender_address));
  sender_address.sin_family = AF_INET;
  sender_address.sin_port = htons(port);
  sender_address.sin_addr.s_addr = htons(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htons 인듯?

  printf("[receiver] binding port with %d\n", port);

  int bind_status = ::bind(sock, (struct sockaddr *)&sender_address, sizeof(sender_address));
  if (bind_status < 0)
  {
    printf("binding port with %d\n", port);
    perror("[receiver] bind");
    exit(EXIT_FAILURE);
  }

  printf("[receiver] now binded\n");

  if (listen(sock, MAX_NUM_PUBLISHER) < 0)
  {
    perror("[receiver] listen");
    exit(EXIT_FAILURE);
  }

  printf("[receiver] now listening\n");

  int subscriber_sock;
  if ((subscriber_sock = accept(sock, NULL, NULL)) < 0)
  {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  printf("[receiver] now connected with subscriber\n");

  char topic[MAX_TOPIC_LENGTH];
  while (true)
  {
    memset(topic, '\0', MAX_TOPIC_LENGTH);
    if (recv(subscriber_sock, topic, MAX_TOPIC_LENGTH, 0) < 0)
    {
      perror("recv");
      exit(EXIT_FAILURE);
    }
    else
    {
      printf("received topic to (un)subscribe from user\n");
      // topic 을 받으면, 추가하던가 삭제하던가 해준다.
      // pthread_mutex_lock(&mutex_sender);

      if (user_informations.find(id) == user_informations.end())
      {
        // id가 없었다면, 처음으로 추가해준다
        user_informations[id] = map<string, pair<bool, int>>();
      }

      if (user_informations[id].find(topic) == user_informations[id].end())
      {
        // id 는 있는데 해당 토픽이 없었다면, 처음으로 추가해주고 true, 0 으로 만들어준다.
        user_informations[id][topic] = {true, 0};
      }
      else
      {
        // 있었다면, toggle 해준다.
        bool isSubscribed = user_informations[id][topic].first;
        if (isSubscribed)
        {
          user_informations[id][topic].first = false;
        }
        else
        {
          user_informations[id][topic].first = true;
          user_informations[id][topic].second = 0;
        }
      }
      pthread_cond_broadcast(&p_thread_cond);
    }
  }
}

Message *newMessage()
{
  Message *message = (Message *)malloc(sizeof(Message));
  memset(message->topic, '\0', MAX_TOPIC_LENGTH);
  memset(message->contents, '\0', MAX_FILE_LENGTH);
  return message;
}

int register_message(Message *message)
{
  // mutex_message lock 잡은채로 진입
  map<string, vector<string>>::iterator it = topics.find(message->topic);
  if (it != topics.end())
  {
    // found
    it->second.push_back(message->contents);
  }
  else
  {
    vector<string> v;
    v.push_back(message->contents);
    topics[message->topic] = v;
  }

  list_all_topics_and_contents();

  return 0; // on success
}

void list_all_topics_and_contents()
{
  cout << endl
       << "All Topics & Contents" << endl;
  for (auto topic : topics)
  {
    cout << "Topic : " << topic.first << endl;
    for (auto content : topic.second)
    {
      cout << content << endl;
    }
    cout << endl;
  }
}

/// send if needed, update idx
int distribute_message(string topic)
{
  printf("distributing messages...\n");

  map<string, vector<string>>::iterator it = topics.find(topic);
  if (it == topics.end())
  {
    // some error
    printf("topic not exists\n");
    return -1;
  }

  // wake all threads
  pthread_cond_broadcast(&p_thread_cond);
  return 0;
}