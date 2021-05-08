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

#define SERVER_IP_ADDRESS (127.0.0.1)
#define SERVER_CONTROL_PORT (21) // custom port number
#define MAX_BUFFER_SIZE (1024)
#define MAX_LEN_COMMAND (5)
#define MAX_RELATIVE_PATH_LENGTH (2021)

const char whitespaces[] = " \t\n\v\f\r";

// Message Queue
typedef struct Message{ // reusable
	int status;
	char command[MAX_LEN_COMMAND];
	char body[MAX_BUFFER_SIZE];
}Message;

enum Command{
	user, 
	pass, 
	nlst, 
	pasv, 
	retr, 
	cwd , 
	quit, 
	type, 
	port, 
	unknown
};

void handlerOnConnectionBroken(int s);
void setConnectionBrokenHandler();
void handle_error(char*);
Message* newMessage(char*, char*);
enum Command interpret(char*);
void callback(enum Command, Message*);
char* toVerbal(int);
int extractClass(int);
void toLowercase(char*);

// data Thread
pthread_t dataThread;
int dataThreadID, status;

// mutex
pthread_mutex_t gmutex; // = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gcond; // = PTHREAD_COND_INITIALIZER;

int requestedPort;
char serverData[MAX_BUFFER_SIZE];

int cfd, data_server_fd;
struct sockaddr_in addr, data_server_addr;
socklen_t addr_size;

char fileName[MAX_RELATIVE_PATH_LENGTH];

bool dataRequestApproved = false;
enum Command sentCommand = user;

int main()
{
	setConnectionBrokenHandler();

  pthread_mutex_init(&gmutex, NULL);
  pthread_cond_init(&gcond, NULL);

	// miscellaneous
	char* userInput;

	cfd = socket(AF_INET, SOCK_STREAM, 0);
	if (cfd == -1)
    handle_error("socket");

	// server side Address setting
	memset(&addr, 0, sizeof(addr));
 	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_CONTROL_PORT);
	addr.sin_addr.s_addr = htonl (INADDR_ANY);

 	addr_size = sizeof(struct sockaddr_in);
	if(connect(cfd, (struct sockaddr*) &addr , addr_size) == -1)
		handle_error("connect");

	printf("Connected to Server\n");
	while(1){
		userInput = malloc(sizeof(char)*MAX_BUFFER_SIZE);
		Message *req = malloc(sizeof *req); 
		Message *res = malloc(sizeof *res); 
		memset(req, 0, sizeof(*req));
		memset(res, 0, sizeof(*res));

		// recieve response
		if(recv(cfd, res, sizeof(*res),0) < 0){
			printf("hello\n");
			sleep(10);
		}
		printf("svr> %d %s %s\n",res->status, toVerbal(res->status), res->body);

		// processing
		callback(sentCommand, res);
		
		// to clean-use screen, wait a second before ftp > appears
		struct timespec reqtime;
		reqtime.tv_sec = 0;
		reqtime.tv_nsec = 100000000;
		nanosleep(&reqtime, NULL);

		printf("ftp> "); scanf(" %1023[^\n]", userInput);					// get oneline
		char* inputCommand = strtok (userInput," \t\n\v\f\r"); 		// extract first word
		enum Command command = interpret(inputCommand); 					// interpret the word
		while(command == unknown){
			printf("unknown command \"%s\"\n",inputCommand);
			memset(req, 0, sizeof(*req));
			printf("ftp> "); scanf(" %1023[^\n]", userInput); 			// get oneline
			char* inputCommand = strtok (userInput," \t\n\v\f\r"); 	// extract first word
			command = interpret(inputCommand);											// interpret the word
		}

		strcpy(req->command,inputCommand);
		char* left = strtok(NULL," \t\n\v\f\r");
		if(left == NULL) left = "\0"; // handle empty body
		strcpy(req->body,left); // leftover
		sentCommand = command;
		if(sentCommand == retr){
			memset(fileName,0,MAX_RELATIVE_PATH_LENGTH);
			strcpy(fileName,left);
		}
		send(cfd, req, sizeof(*req),0);
	}
	return 0;
}

int extractClass(int status){
	return status / 100;
}

// 쓰레드 함수
void *dataThreadRoutine(void *data)
{
	// parsing 해서 사용하는것은 스레드의 몫
	int dataPort;
	if(sentCommand == pasv){
	  char* addressInformation = (char*)data;
	  char* messagePart = strtok(addressInformation,"|"); // printf("messagePart: %s\n",messagePart);
	  char* addressPart = strtok(NULL, "|"); // printf("addressPart: %s\n",addressPart);

		char* pch = strtok (addressPart,", \t\n\v\f\r");
	  int idx = 0, IPAdress = 0; dataPort = 0;
	  while (pch != NULL)
	  {
	    printf ("[%d] %s\n", idx, pch);
	    if(strlen(pch) > 3) {
	    	printf("wrong ip:dataPort given\n");
	    	return NULL;
	    }else {
		    int value = atoi(pch);
			  if(value){
			    if(idx < 4){
			    	IPAdress *= 256;
			    	IPAdress += value;

			  	}else{
			  		dataPort *= 256;
			  		dataPort += value;
			  	}
			  }
			  pch = strtok (NULL,", \t\n\v\f\r");
	  	}
	  	idx++;
	  }
	}else if (sentCommand == port){
	  char* addressInformation = (char*)data;
	  char* messagePart = strtok(addressInformation,":"); // printf("messagePart: %s\n",messagePart);
	  char* portPart = strtok(NULL, ":"); // printf("addressPart: %s\n",addressPart);
	  dataPort = atoi(portPart);
	  printf("dataPort : %d\n",dataPort);
	}

	// data socket [client]
	data_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (data_server_fd == -1)
    handle_error("socket");

  // printf("data socket created\n");

	// data socket [server]
	memset(&data_server_addr, 0, sizeof(struct sockaddr_in));
 	data_server_addr.sin_family = AF_INET;
	data_server_addr.sin_port = htons(dataPort);
	data_server_addr.sin_addr.s_addr = htonl (INADDR_ANY);

  pthread_cond_wait(&gcond, &gmutex);

  // printf("data thread woke up\n");

	if(connect(data_server_fd, (struct sockaddr*) &data_server_addr , sizeof(struct sockaddr_in)) == -1)
		handle_error("connect");

  // printf("data socket connected\n");

  // anyway, get 
	Message *res = malloc(sizeof *res); 
	memset(res, 0, sizeof(*res));
	recv(data_server_fd, res, sizeof(*res), 0);
  strcpy(serverData, res->body);

  // processing
  switch (sentCommand){
  	case retr:{
  		// save to current folder.
  		FILE *wfp;
  		strcat(fileName,"fromServer");
  		if((wfp = fopen(fileName, "w")) == NULL) {
				return NULL;
			}
			fwrite(serverData, sizeof(char), strlen(serverData), wfp);
			fclose(wfp);
  		break;
  	}
  	case nlst:{
  		// parse using |
  		printf("\n");
  		char* token = strtok(serverData,"| \t\n\v\f\r");
  		while(token != NULL){
  			printf(" %s\n",token);
				token = strtok(NULL,"| \t\n\v\f\r");		
  		}
  		printf("\n");
  		break;
  	}
  	default:
  		close(data_server_fd);
			return NULL;
	}

	close(data_server_fd);
  return (void*)"Success!";
}

void callback(enum Command sentCommand, Message* responseMessage) {
	switch(sentCommand){
		case user:
		case pass: 
		case cwd:
		case type:
			break;
		case nlst:
			switch(extractClass(responseMessage->status)){
				case 1: {// actually, 150
					// success
					// opened channeling
					// thread do something
			    int status;

					// wake up and sleep
					// dataRequestApproved = true;
					memset(serverData, 0, MAX_BUFFER_SIZE);

			    pthread_cond_signal(&gcond);
				  pthread_join(dataThread, (void **)&status);

					// wait for 226
					Message *res = malloc(sizeof *res); 
					memset(res, 0, sizeof(*res));
					recv(cfd, res, sizeof(*res),0);

					// success
					return;
				}
				default:
					// failure
					return;
			}

			// show dir entry

			break;
		case retr:{
			switch(extractClass(responseMessage->status)){
				case 1:{
					// success
					// opened channeling
					// thread do something
			    int status;

					// wake up and sleep
					memset(serverData, 0, MAX_BUFFER_SIZE);

			    pthread_cond_signal(&gcond);
    			pthread_join(dataThread, (void **)&status);

	        // wait for 226
					Message *res = malloc(sizeof *res); 
					memset(res, 0, sizeof(*res));
					recv(cfd, res, sizeof(*res),0);
					
				  // extract data (encapsulated in status variable)
			  	printf("%s\n",serverData);
			  	return;
			  }
				default:
					// failure
					return;
			}
		}
		case quit:
			// cleanup and exit process with EXIT_SUCCESS
			close(cfd);
			exit(EXIT_SUCCESS);
			break;
		case pasv: 
			if ((dataThreadID = pthread_create(&dataThread, NULL, dataThreadRoutine, (void *) responseMessage->body)) == -1 ){
				handle_error("pthread create");
			}
			break;
		case port: 
			if ((dataThreadID = pthread_create(&dataThread, NULL, dataThreadRoutine, (void *) responseMessage->body)) == -1 ){
				handle_error("pthread create");
			}
			break;
		default:
			printf("unknown command\n");
			break;
	}
}

char* toVerbal(int status){
	status /= 100;
	switch(status){
		case 1:
			return "INIT";
		case 2:
			return "OK";
		case 3:
			return "REDIRECT";
		case 4:
			return "error";
		case 5:
			return "server error";
		default:
			return "unknown";
	}
}

void toLowercase(char* inputString){
	int len = strlen(inputString);
	for(int i=0;i<len;i++){
		inputString[i] = tolower(inputString[i]);
	}
}

enum Command interpret(char* inputCommand){
	toLowercase(inputCommand);
	if(strcmp(inputCommand,"user") == 0){
		return user;
	}else if(strcmp(inputCommand, "pass") == 0){
		return pass;
	}else if(strcmp(inputCommand, "nlst") == 0){
		return nlst;
	}else if(strcmp(inputCommand, "pasv") == 0){
		return pasv;
	}else if(strcmp(inputCommand, "retr") == 0){
		return retr;
	}else if(strcmp(inputCommand, "cwd") == 0){
		return cwd;
	}else if(strcmp(inputCommand, "quit") == 0){
		return quit;
	}else if(strcmp(inputCommand, "type") == 0){
		return type;
	}else if(strcmp(inputCommand, "port") == 0){
		return port;
	}else
		return unknown;
}

Message* newMessage(char* command, char* body){
	Message *message = malloc(sizeof *message);
	memset(message, 0, sizeof(*message));
	strcpy(message->command,command);
	strcpy(message->body,body);
	return message;
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
