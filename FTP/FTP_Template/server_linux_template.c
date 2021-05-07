// Linux
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

int listen_sock, clnt_sock[10], data_sock[10];
int control_port = 36007;
int data_port;
char *username = "np2019";
char *password = "np2019";

void *recvThread(void *arg);
int LastIndexof(char *str, char c);
void closedatasocket(int idx);

enum
{
	NOTLOGGEDIN,
	NEEDPASSWORD,
	LOGGEDIN
};

int main()
{
	int count = 0;
	pthread_t th_recv[10];
	data_port = control_port;

	struct sockaddr_in addr;

	// �ּ� ���� 36007

	// ���� ���� TCP IPv4

	// �ּ� ���ε�

	// listen

	while (1)
	{
		// accept and create client thread
	}

	closesocket(listen_sock);
}

void recvThread(void *arg)
{
	int connected = 1;
	int idx = *((int *)arg);

	// get current working folder or directory

	// send 220 message

	// receive command
	while (connected)
	{

		// ������ ���� �� command ó��
		// USER, PASS, CWD, NLST, QUIT, PASV, RETR, TYPE
		// while recv:
		//	commands:
		//		USER: username ��, PASS ����
		//		PASS: password ��
		//		CWD: ���� or ���丮 ����, ���� �۾� ���丮 �ʿ�
		//		QUIT: control ���� ����
		//		PASV: data ���� ����, client���� data ���� ���� ����
		//		RETR: file �б�, ������ ����
		//		TYPE: ���� �б� ��� ����
	}
	close(clnt_sock[idx]);
}

int LastIndexof(char *str, char c)
{
	int i, idx = -1;
	for (i = 0; i < strlen(str); i++)
	{
		if (str[i] == c)
			idx = i;
	}

	return idx;
}
void closedatasocket(int idx)
{
	close(data_sock[idx]);
	data_sock[idx] = NULL;
}
