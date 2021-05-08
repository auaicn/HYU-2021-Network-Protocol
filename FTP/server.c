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

#define SERVER_PORT (12345) // custom port number
#define PASSIVE_PORT (12347) // custom port number
#define MAX_BUFFER_SIZE (1024)
#define MAX_NUM_CLIENT (1000) // no matters
#define MAX_LEN_COMMAND (5)

const char userId[] = "auaicn";
const char userPassword[] = "thislove1!";

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

enum AuthorizationStatus
{
	NOTLOGGEDIN,
	NEEDPASSWORD,
	LOGGEDIN
};

const char whitespaces[] = " \t\n\v\f\r";

// Message Queue
typedef struct Message{
	int status;
	char command[MAX_LEN_COMMAND];
	char body[MAX_BUFFER_SIZE];
}Message;

void handlerOnConnectionBroken(int s);
void setConnectionBrokenHandler();
void handle_error(char*);
Message* newMessage(int, char*);
enum Command interpret(char*);
int execute(Message*, char*);
char* getCurrentIpAddress();
char* changeWorkingDirectory(char*);
void toLowercase(char*);
void getDEntries(char*);

enum Command dataCommand;
enum AuthorizationStatus authStatus;
bool loggedOn = false;
bool validUsername = false;
bool asciiMode = true; // 기본적으로 ascii 모드를 지원한다고 하자.

int sfd, cfd, data_server_fd, data_client_fd;
struct sockaddr_in my_addr, peer_addr, data_server_addr, data_client_addr;

// data Thread
pthread_t dataThread;
int dataThreadID, status;

// mutex
pthread_mutex_t gmutex; // = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gcond; // = PTHREAD_COND_INITIALIZER;

char relativePath[] = "/";
char* fileName;
char* filePath;

int main()
{

  pthread_mutex_init(&gmutex, NULL);
  pthread_cond_init(&gcond, NULL);

	setConnectionBrokenHandler();

	socklen_t peer_addr_size;
	char* responseMessage;

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

	// send Initial Server Connection Success Message
	send(cfd, newMessage(220, "Server ready"), sizeof(Message),0);
	while(true){
		Message *req = malloc(sizeof *req); 
		memset(req, 0, sizeof *req);

		// receive command
		recv(cfd, req, sizeof(Message),0);
		printf("[client] %s --body %s\n", req->command, req->body);

		// processing
		int status = execute(req, responseMessage);

		// send response
		send(cfd, newMessage(status,responseMessage), sizeof(Message),0);
	}

	close(cfd);
	return 0;
}

int execute(Message* req, char* responseMessage){
	memset(responseMessage,0,MAX_BUFFER_SIZE);
	enum Command command = interpret(req->command);
	switch(command){
		case user:
			if (strcmp(req->body,userId) == 0){
				authStatus = NEEDPASSWORD;
				sprintf(responseMessage, "Password required for %s", userId);
				return 331;
			}else{
				authStatus = NOTLOGGEDIN;
				responseMessage = "wrong username. try again";
				return 400;
			}
		case pass:{
			switch(authStatus){
				case NOTLOGGEDIN:
					responseMessage = "enter user name";
					return 400;
					break;
				case NEEDPASSWORD:
					if (strcmp(req->body,userPassword) == 0){
						responseMessage = "Logged on";
						authStatus = LOGGEDIN;
						return 230;
					}else{
						responseMessage = "wrong password. try again";
						return 400;
					}
					break;
				case LOGGEDIN:
					sprintf(responseMessage, "already logined as \"%s\"", userId);
					return 400;
					break;						
				}
			}
			break;
		case nlist:{
			if(authStatus != LOGGEDIN){
				responseMessage = "operation not authroized. Please login first.";
				return 401;
			}
			// set command type, wake up and sleep
			dataCommand = nlist;
			pthread_cond_signal(&gcond);

			char* intermediateMessage[] = (char*) malloc(sizeof(char)*MAX_BUFFER_SIZE);
			sprintf(intermediateMessage, "Opening data channel of directory listing of %s", relativePath);
			send(dfd, newMessage(150, responseMessage), sizeof(Message),0);

			// no receive send again
			pthread_join(dataThread, (void **)&status);

			if(status == NULL){
				//cleanup
		   	close(fp);
		   	printf("Internal Server Error\n");
     		sprintf(responseMessage, "Internal Server Error\n");
     		return 500;
			}

			// success
     	sprintf(responseMessage, "Successfully transferred %s\n", relativePath);
			return 226
		}
		case retr:{
			if(authStatus != LOGGEDIN){
				responseMessage = "operation not authroized. Please login first.";
				return 401;
			}

			// set global path variable
			fileName = strtok(req->body," \t\n\v\f\r"); // truncate white space
			filePath = concat(relativePath, fileName);
			FILE *fp;
			if((fp = fopen(filePath, "r")) == NULL){
				//cleanup
				fclose(fp);
				sprintf(responseMessage, "failed opening %s",filePath);
				return 404;
			}
			// set command type, wake up and sleep
			dataCommand = retr;
			pthread_cond_signal(&gcond);

			char* intermediateMessage[] = (char*) malloc(sizeof(char)*MAX_BUFFER_SIZE);
			sprintf(intermediateMessage,"Opening data Channel of file donwload from server of \"%s\"",fileName);
			send(dfd, newMessage(150,responseMessage), sizeof(Message),0);

			// no receive
			pthread_join(dataThread, (void **)&status);

			if(status == NULL){
				//cleanup
		   	close(fp);
		   	printf("Internal Server Error\n");
     		sprintf(responseMessage, "Internal Server Error\n");
     		return 500;
			}

			// success
     	sprintf(responseMessage, "Successfully transferred %s\n", fileName);
			return 226;
		}
		case cwd: // change working directory. same as `cd`
			if(authStatus != LOGGEDIN){
				responseMessage = "operation not authroized. Please login first.";
				return 401;
			}

			char* newRelativePath;
			if((newRelativePath = changeWorkingDirectory(req->body)) == NULL){
				// current path change error
				sprintf(responseMessage, "changing directory failed. current working directory is \"%s\"",relativePath);
				return 404;
			}

			// success
			sprintf(responseMessage, "changed current working directory to \"%s\"",newRelativePath);
			return 226;
		case quit:
			// success
			responseMessage = "Goodbye";
			authStatus = NOTLOGGEDIN; // 아예 꺼지기때문에 상관없긴 하다.
			return 221;
		case type:
			if(authStatus != LOGGEDIN){
				responseMessage = "operation not authroized. Please login first.";
				return 401;
			}

			// success
			if(tolower((req->body)[0]) == "i"){
				asciiMode = false;
				// binary file transfer mode
				sprintf(responseMessage, "Type set to %s (Binary)", req->body);
			}else{
				// ascii file transfer mode
				asciiMode = true;
				sprintf(responseMessage, "Type set to %s (Ascii)", req->body);
			}
			return 200;
		case pasv:
			if(authStatus != LOGGEDIN){
				responseMessage = "operation not authroized. Please login first.";
				return 401;
			}

			/*
			// we have to find available port
			int portCandidate = SERVER_PORT;

			if((dfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				handle_error("socket");

			data_addr.sin_addr.s_addr = 0;
			data_addr.sin_addr.s_addr = INADDR_ANY;
			data_addr.sin_family = AF_INET;

			while(++portCandidate){
				data_addr.sin_port = htons(portCandidate);

				// try bidning
				if (bind(dfd, (struct sockaddr *)&data_addr, sizeof(struct sockaddr_in)) == -1) {
			  	if (errno == EADDRINUSE){
			    	printf("Port %d is already in use\n", portCandidate);
			  	}
					continue;
				}
				break;
			}
			*/

			int port = PASSIVE_PORT;

			printf("requested portNumber : %d\n",port);

		  // create thread with portnumber
			if ((dataThreadID = pthread_create(&dataThread, NULL, dataThreadRoutine, (void *) port)) == -1 ){
				responseMessage = "Internal Server Error";
				return 500;
			}

			sprintf(responseMessage,"Entering Passive Mode (IPADDRESS,%d,%d)", port/256, port%256);
			return 227;

		case port:
			if(authStatus != LOGGEDIN){
				responseMessage = "operation not authroized. Please login first.";
				return 401;
			}

		  char* pch = strtok (req->body,", \t\n\v\f\r");
		  int idx = 0, IPAdress = 0, port = 0;
		  while (idx++ && pch != NULL)
		  {
		  	// do stuff with pch
		    printf ("[%d]%s\n", idx, pch);
		    if(strlen(pch) > 3) {
		    	responseMessage = "invalid port number encountered. Try Agiain"
		    	return 400;
		    }else {
			    int value = atoi(pch);
		    if(value)
			    if(idx < 4){
			    	IPAdress *= 256;
			    	IPAdress += value;

			  	}else{
			  		port *= 256;
			  		port += value;
			  	}
		    }
		    pch = strtok (NULL, ", \t\n\v\f\r");
		  }

		  printf("requested portNumber : %d\n",port);

		  // create thread with portnumber
			if ((dataThreadID = pthread_create(&dataThread, NULL, dataThreadRoutine, (void *) port)) == -1 ){
				responseMessage = "Internal Server Error";
				return 500;
			}

			responseMessage = "Port command successful";
			return 200;
		default:
			responseMessage = "unknown command";
			return 404;
	}
	return -1;
}

/// make socket with given port(data) then sleep until RETR or NLST command is given
void *dataThreadRoutine(void *data){
	
	// data socket opening
	data_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (data_server_fd == -1)
        handle_error("socket");

	// server side Address setting
 	int port = (int) data;
	memset(&data_server_addr, 0, sizeof(data_server_addr));
 	data_server_addr.sin_family = AF_INET;
	data_server_addr.sin_port = htons(port);
	data_server_addr.sin_addr.s_addr = htonl (INADDR_ANY);

	if (bind(data_server_fd, (struct sockaddr *)&data_server_addr, sizeof(struct sockaddr_in)) == -1) {
	  if (errno == EADDRINUSE){
	  	handle_error("bind port already in use");
	  	return NULL;
	  }
	}

  printf("data socket [server] created\n");
  printf("data thread gone sleep\n");
  pthread_cond_wait(&gcond, &gmutex);
  printf("data thread woke up\n");
  if(!dataRequestApproved){
  	// not approved operation then distroy thread
  	// main thread is waiting (joining)
  	return NULL;
  }

  printf("data thread socket now listening on PORT: %d\n", port);

	if(listen(data_server_fd, MAX_NUM_CLIENT) == -1)
		handle_error("listen");
	else
		printf("data thread socket connectd\n");

 	int data_client_addr_size = sizeof(struct sockaddr_in);
  data_client_fd = accept(data_server_fd, (struct sockaddr *) &data_client_addr, &data_client_addr_size);
  if (data_client_fd == -1)
      handle_error("accept");
  else
  	printf("client accepted\n");

  // 여기서 분기된다.
  switch (dataCommand){
  	case RETR:
  		

  		break;
  	case NLST:

  		Message *res = malloc(sizeof *res); 
			memset(res, 0, sizeof(*res));
			send(data_server_fd, res, sizeof(*res),0);
			send(data_server_fd, newMessage(150, "Server ready"), sizeof(Message),0);

			sprintf(intermediateMessage,"Opening data Channel of file donwloading from server of %s",filePath);

		  strcpy(serverData, res->body);

  		// give 150 to client

  		// processing

  		return (void*)"Success"; // 알아서 226 은 가게 된다.
  	default:
  		return NULL;
  }
}

char* changeWorkingDirectory(char* relativePath){
	// get relative Path
	char* s = strtok(req->body," \t\n\v\f\r"); // truncate white space
	char *newPath[] = concat(relativePath, s)
	FILE *fp;
	if((fp = fopen(newPath, "w")) == NULL){
		sprintf(responseMessage, "failed opening %s",relativePath);
		return NULL;
	}
	// File opened
	relativePath = newPath;
}

char* getCurrentIpAddress(){
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

	ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);

	/* display result */
	// printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

}

Command interpret(char* inputCommand){
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

void toLowercase(char* inputString){
	int len = strlen(inputString);
	for(int i=0;i<len;i++){
		inputString[i] = tolower(inputString[i]);
	}
}

Message* newMessage(int status, char* body){
	Message *message = malloc(sizeof *message);
	memset(message, 0, sizeof(*message));
	message->status = status;
	strcpy(message->body,body);
	return message;
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
