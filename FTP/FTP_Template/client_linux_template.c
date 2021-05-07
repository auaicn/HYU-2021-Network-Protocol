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

void ConnectToServer();
void *recvDataThread();

int mode, port;
char buff[1024];
char ip_address[16];
char local_ip_address[16];
char username[31];
char password[31];
char command[100];

FILE *fp;

int FileDownMode = 0;

int sock_main;

struct sockaddr_in addr, addr_data;

int main()
{
	char tmp[40] = {0};

	struct ifaddrs *addrs, *test;

	getifaddrs(&addrs);
	test = addrs;

	while (test)
	{
		if (test->ifa_addr && test->ifa_addr->sa_family == AF_INET && strcmp(test->ifa_name, "lo"))
		{
			struct sockaddr_in *pAddr = (struct sockaddr_in *)test->ifa_addr;
			strcpy(local_ip_address, inet_ntoa(pAddr->sin_addr));
		}
		test = test->ifa_next;
	}
	freeifaddrs(addrs);

	for (int i = 0; i < 16; i++)
	{
		if (local_ip_address[i] == '.')
			local_ip_address[i] = ',';
	}

	printf("1. Server, 2. Client: ");
	scanf("%d", &mode);
	getchar();

	if (mode == 1)
	{
	}
	else if (mode == 2)
	{													// client
		printf("id address: "); // ���� ������
		scanf("%s", ip_address);
		printf("port: "); // ���� ��Ʈ
		scanf("%d", &port);
		printf("User name: "); // User name
		scanf("%s", username);
		printf("password: "); // Password
		scanf("%s", password);
		getchar();

		ConnectToServer(); // Connect to Server

		while (1)
		{
			printf("ftp> ");
			memset(command, 0, sizeof(command));
			fgets(command, 1000, stdin);
			command[strlen(command) - 1] = '\0';
			memset(tmp, 0, sizeof(tmp));

			// ���� Ŀ�ǵ� ó�� �� FTP Ŀ�ǵ� ����
			// quit	-> QUIT
			// ls	-> NLST
			// cd	-> CWD "directory"
			// get	-> RETR "file name", file �ٿ�ε� �غ�
			// bin	-> TYPE I
			// ascii-> TYPE A

			// ���� �޽��� ó��
			// 200, 211, 221, 226, 250, 425
			// Active Mode ó��
			// Ŀ�ǵ�: PORT IP,PORT
		}
	}

	return 0;
}

void ConnectToServer()
{
	// �ּ� ����

	// Control ���� ����

	// ���� ����

	while (1)
	{
		// �α��� �޽��� ó��
		// Ŀ�ǵ�: USER, PASS; ����: 200, 220, 230, 331
		// Active Mode ó��
		// Ŀ�ǵ�: PORT IP,PORT
	}
}

void *recvDataThread()
{
	// �ּ� ����

	// Data ���� ����

	// Data ���� �ּ� bind

	// listen �� accept

	while (1)
	{
		// FTP Ŀ�ǵ� ��� (ls, cd, get) ���� ó��
	}
}
