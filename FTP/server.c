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

#define SERVER_CONTROL_PORT (21) // custom port number
#define PASSIVE_PORT (12347) // custom port number
#define MAX_BUFFER_SIZE (1024)
#define MAX_DIRENT_LENGTH (1024)
#define MAX_NUM_CLIENT (1000) // no matters
#define MAX_LEN_COMMAND (5)
#define MAX_RELATIVE_PATH_LENGTH (2021)

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
	LOGGEDIN,
	MODESELECTED,
};

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
void *dataThreadRoutine(void*);
bool validDirectoryEntry(char*);
char* getDirectoryEntries();
char* getStreamFromFile();

enum Command dataCommand = unknown; // strong typing
enum AuthorizationStatus authStatus = NOTLOGGEDIN; // to submit
// enum AuthorizationStatus authStatus = LOGGEDIN; // backdoor
bool asciiMode = true; // 기본적으로 ascii 모드를 지원한다고 하자.

int sfd, cfd, data_server_fd, data_client_fd;
struct sockaddr_in my_addr, peer_addr, data_server_addr, data_client_addr;

// data Thread
pthread_t dataThread;
int dataThreadID, status;

// mutex
pthread_mutex_t gmutex; // = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gcond; // = PTHREAD_COND_INITIALIZER;

char relativePath[MAX_RELATIVE_PATH_LENGTH];
char* fileName;
int ports[] = {1025,1026,1027,1028,1197,1235,1236,12347,12348,12342,4321,2311,1239,1184, 1216, 1193, 1143, 1266}; // 1024번 이후의 임의의 포트
int portIdx = 0;

int main()
{

	signal(SIGPIPE, SIG_IGN);

	relativePath[0] = '.';
	relativePath[1] = '/';

  pthread_mutex_init(&gmutex, NULL);
  pthread_cond_init(&gcond, NULL);

	// setConnectionBrokenHandler();

	socklen_t peer_addr_size;
	char* responseMessage;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
    handle_error("socket");

	memset(&my_addr, 0, sizeof(struct sockaddr_in));

 	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(SERVER_CONTROL_PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // <- 결국 0이다 INADDR_ANY 그리고 0을 처리해주는건 htonl 인듯?
  	
	// casting INET specific server-side sock address into general sock addr
	if(bind(sfd, (struct sockaddr *) &my_addr, sizeof(my_addr)) == -1)
		handle_error("bind");

	while(true){

		if(listen(sfd, MAX_NUM_CLIENT) == -1)
			handle_error("listen");

		printf("server is now listening on PORT: %d\n",SERVER_CONTROL_PORT);

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
			if(recv(cfd, req, sizeof(Message),0) < 0){
				close(cfd);
				sleep(1);
				break;
			}
			printf("[client] %s --body %s\n", req->command, req->body);

			// processing
			int status = execute(req, responseMessage);

			// send response
			send(cfd, newMessage(status,responseMessage), sizeof(Message),0);
		}
	}

	close(cfd);
	return 0;
}

int execute(Message* req, char* responseMessage){
	memset(responseMessage,0,MAX_BUFFER_SIZE);
	enum Command command = interpret(req->command);
	switch(command){
		case user:
			printf("user");
			if(authStatus == NOTLOGGEDIN){
				if (strcmp(req->body,userId) == 0){
					authStatus = NEEDPASSWORD;
					sprintf(responseMessage, "Password required for \"%s\"", userId);
					return 331;
				}else{
					authStatus = NOTLOGGEDIN;
					sprintf(responseMessage,"wrong username. try again");
					return 400;
				}
			}else{
				sprintf(responseMessage,"user id already given");
				return 400;
			}
		case pass:{
			printf("pass");
			switch(authStatus){
				case NOTLOGGEDIN:
					sprintf(responseMessage,"enter user name");
					return 400;
					break;
				case NEEDPASSWORD:
					if (strcmp(req->body,userPassword) == 0){
						sprintf(responseMessage,"Logged on");
						authStatus = LOGGEDIN;
						return 230;
					}else{
						sprintf(responseMessage,"wrong password. try again");
						return 400;
					}
					break;
				case LOGGEDIN:
				case MODESELECTED:
					sprintf(responseMessage, "already logined");
					return 200;
					break;						
				}
			}
			break;
		case nlst:{
			if(authStatus != MODESELECTED){
				sprintf(responseMessage,"please select ftp mode first (active, passive)");
				return 401;
			}
			// set command type, wake up and sleep
			dataCommand = nlst;
			pthread_cond_signal(&gcond);

			char* intermediateMessage = (char*) malloc(sizeof(char)*MAX_BUFFER_SIZE);
			sprintf(intermediateMessage, "Opening data channel of directory listing of %s", relativePath);
			send(cfd, newMessage(150, responseMessage), sizeof(Message),0);

			// no receive send again
			int status;
			pthread_join(dataThread, (void **)&status);

			if(status == 0){
				//cleanup
		   	printf("Internal Server Error\n");
     		sprintf(responseMessage, "Internal Server Error\n");
     		return 500;
			}

			// success
			authStatus = LOGGEDIN;
     	sprintf(responseMessage, "Successfully transferred %s\n", relativePath);
			return 226;
		}
		case retr:{
			if(authStatus != MODESELECTED){
				sprintf(responseMessage,"please select ftp mode first (active, passive)");
				return 401;
			}

			if(!validDirectoryEntry(fileName = strtok(req->body," \t\n\v\f\r"))){
				sprintf(responseMessage, "failed opening %s",fileName);
				return 404;
			}

			// set command type, wake up and sleep
			dataCommand = retr;
			pthread_cond_signal(&gcond);

			char* intermediateMessage = (char*) malloc(sizeof(char)*MAX_BUFFER_SIZE);
			sprintf(intermediateMessage,"Opening data Channel of file donwload from server of \"%s\"",fileName);
			send(cfd, newMessage(150,responseMessage), sizeof(Message),0);

			// no receive
			int status;
			pthread_join(dataThread, (void **)&status);

			if(status == 0){
				//cleanup
		   	printf("Internal Server Error\n");
     		sprintf(responseMessage, "Internal Server Error\n");
     		return 500;
			}

			// success
			authStatus = LOGGEDIN;
     	sprintf(responseMessage, "Successfully transferred %s\n", fileName);
			return 226;
		}
		case cwd: // change working directory. same as `cd`
			if(authStatus != LOGGEDIN){
				sprintf(responseMessage,"operation not authroized. Please login first.");
				return 401;
			}

			if(changeWorkingDirectory(req->body) == NULL){
				// current path change error
				sprintf(responseMessage, "changing directory failed. current working directory is \"%s\"", relativePath);
				return 500;
			}

			// success
			sprintf(responseMessage, "changed current working directory to \"%s\"", relativePath);
			return 226;
		case quit:
			// success
			sprintf(responseMessage,"Goodbye");
			authStatus = NOTLOGGEDIN; // 아예 꺼지기때문에 상관없긴 하다.
			return 221;
		case type:
			if(authStatus != LOGGEDIN){
				sprintf(responseMessage,"operation not authroized. Please login first.");
				return 401;printf("please select ftp mode first (active, passive)");
			}
			// success
			if(strlen(req->body) != 1){
				sprintf(responseMessage, "type must be a single character \"%s\" is not a single character.", req->body);
				return 401;
			}
			if(tolower(req->body[0]) == 'i'){
				asciiMode = false;
				// binary file transfer mode
				sprintf(responseMessage, "Type set to %s (Binary)", req->body);
			} else if(tolower(req->body[0]) == 'a'){
				// ascii file transfer mode
				asciiMode = true;
				sprintf(responseMessage, "Type set to %s (Ascii)", req->body);
			}else{
				sprintf(responseMessage, "invalid file transfer type \"%s\"", req->body);
				return 401;
			}
			return 200;
		case pasv:{
			if(authStatus != LOGGEDIN){
				sprintf(responseMessage,"operation not authroized. Please login first.");
				return 401;
			}

			int port = ports[portIdx++];

		  // create thread with portnumber
			if ((dataThreadID = pthread_create(&dataThread, NULL, dataThreadRoutine, (void *) port)) == -1 ){
				sprintf(responseMessage,"Internal Server Error");
				return 500;
			}

			sprintf(responseMessage,"Entering Passive Mode | 192,168,0,1,%d,%d", port/256, port%256);
			authStatus = MODESELECTED;
			return 227;
		}
		case port:{
			if(authStatus != LOGGEDIN){
				sprintf(responseMessage,"operation not authroized. Please login first.");
				return 401;
			}

		  char* pch = strtok (req->body,", \t\n\v\f\r");
		  int idx = 0, IPAdress = 0, port = 0;
		  while (pch != NULL)
		  {
		  	// do stuff with pch
		    // printf ("[%d]%s\n", idx, pch);
		    if(strlen(pch) > 3) {
		    	sprintf(responseMessage,"invalid port number encountered. Try Agiain");
		    	return 400;
		    }else {
			    int value = atoi(pch);
			    if(value){
				    if(idx < 4){
				    	IPAdress *= 256;
				    	IPAdress += value;

				  	}else{
				  		port *= 256;
				  		port += value;
				  	}
				  }
		    }
		    pch = strtok (NULL, ", \t\n\v\f\r");
		    idx++;
		  }

		  printf("requested portNumber : %d\n",port);

		  // create thread with portnumber
			if ((dataThreadID = pthread_create(&dataThread, NULL, dataThreadRoutine, (void *) port)) == -1 ){
				sprintf(responseMessage,"Internal Server Error");
				return 500;
			}

			sprintf(responseMessage,"Port command successful :%d",port);
			authStatus = MODESELECTED;
			return 200;
		}
		default:
			sprintf(responseMessage,"unknown command");
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

  // printf("data socket [server] created\n");
  // printf("data thread gone sleep\n");
  pthread_cond_wait(&gcond, &gmutex);
  // printf("data thread woke up\n");
  
  // if(!dataRequestApproved){
  // 	// not approved operation then distroy thread
  // 	// main thread is waiting (joining)
  // 	return NULL;
  // }

  printf("data thread socket now listening on PORT: %d\n", port);

	if(listen(data_server_fd, MAX_NUM_CLIENT) == -1)
		handle_error("listen");
	// else
	// 	printf("data thread socket connected\n");

 	socklen_t data_client_addr_size = sizeof(struct sockaddr_in);
  data_client_fd = accept(data_server_fd, (struct sockaddr *) &data_client_addr, &data_client_addr_size);
  if (data_client_fd == -1)
      handle_error("accept");
  // else
  // 	printf("client accepted\n");

  // 여기서 분기된다.
  switch (dataCommand){
   	case retr:{
			send(data_client_fd, newMessage(-1, getStreamFromFile()), sizeof(Message), 0);
  		return (void*) "Success";
  	}
  	case nlst:{
			send(data_client_fd, newMessage(-1, getDirectoryEntries()), sizeof(Message), 0);
  		return (void*) "Success"; // 알아서 226 은 가게 된다.
  	}
  	default:
  		// unkown dataCommand Set
  		return NULL;
  }
}

char* getStreamFromFile(){
	char* serverData = malloc(MAX_BUFFER_SIZE);
	char* filePath = (char*)malloc(MAX_RELATIVE_PATH_LENGTH);
	memset(filePath,0,MAX_RELATIVE_PATH_LENGTH);
	strcpy(filePath,relativePath);
	strcat(filePath,fileName);
	printf("filePath : %s\n",filePath);
	FILE *rfp;
	if((rfp = fopen(filePath, "r")) == NULL) {
		return NULL;
	}
	fread(serverData, sizeof(char), MAX_BUFFER_SIZE, rfp);
	fclose(rfp);
	free(filePath);
	return serverData;
}

bool validDirectoryEntry(char* entryToTest){
	DIR *currentWorkingDirectory = opendir(relativePath);
  struct dirent *entry;
  bool found = false;
  if (currentWorkingDirectory) {
    while ((entry = readdir(currentWorkingDirectory)) != NULL) {
      // printf("%s\n", entry->d_name);
    	if(strcmp(entry->d_name, entryToTest))
    		found = true;
    }
    closedir(currentWorkingDirectory);
  }else{
  	handle_error("opendir");
  }
  return found;
}

char* changeWorkingDirectory(char* body){
	// get relative dir
	char* dirName = strtok(body," \t\n\v\f\r"); // truncate white space
	if(!validDirectoryEntry(dirName)){ return NULL; }
	strcat(relativePath, dirName);
	return relativePath;
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

char* getDirectoryEntries(){
	DIR *dir;
	struct dirent *entry;
	char* entries = (char*)malloc(MAX_BUFFER_SIZE);
	memset(entries, 0, MAX_BUFFER_SIZE);
	if ((dir = opendir(relativePath)) == NULL){
		perror("opendir() error");
	}else{
		while ((entry = readdir(dir)) != NULL){
			char* entryName = malloc(MAX_DIRENT_LENGTH);
			memset(entryName,0,MAX_DIRENT_LENGTH);
			entryName[0] = '|'; // preprocessing
			strcpy(entryName+1,entry->d_name);
			strcat(entries,entryName);
		}
		closedir(dir);
	}
	printf("entries : %s\n",entries);
	return entries;
}

void handlerOnConnectionBroken(int s) {
	handle_error("[SIGPIPE] connection broken");
}

void handleSigpipe(){

}

void setConnectionBrokenHandler(){
	signal(SIGPIPE, handleSigpipe); // to prevent broken TCP connection, set signal handler
}

void handle_error(char* message){
	perror(message);
	exit(EXIT_FAILURE);
}
