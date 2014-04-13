#ifndef  _CONNECT_TO_SERVER
#define  _CONNECT_TO_SERVER

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "../Utils/base64.h"

#define SERVER "66.219.103.59"
#define SERVER_PORT 9999

int global_socket;
char originBuf[20], destBuf[20];

int connect_to_server();
int connect_with_timeout(char *host, int port, int timeout_sec,
		int timeout_usec, char *account);
int wait_android_request();
#endif
