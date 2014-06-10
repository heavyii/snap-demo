#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <paths.h>
#include<sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/tcp.h>
#include "packet.h"
#include "socket.h"
#include "misc.h"

//server listenning port
#define SERVERPORT 5005

//maximum connection
#define MAXCONNECT 10

//define the client state
#define DISCONNECT     0
#define CONNECTED      1
#define SNIFFER_OPEN   2
#define SNIFFER_CLOSE  3 

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

typedef struct _CLIENT {
	int sockfd;
	char ip[32];
} CLIENT;

/*
 * heart_beat_check --check network state
 * @iSockfd the network socket fd
 * @iSockAttrOn  a flag to set socket keepalive
 * @iIdleTime how long did it begin check network at first?
 * @iInterval how long did it begin check network next time?
 * @iCount how much time did it check network before return data?
 */
static void heart_beat_check(int iSockfd, socklen_t iSockAttrOn,
		socklen_t iIdleTime, socklen_t iInterval, socklen_t iCount) {

	if (setsockopt(iSockfd, SOL_SOCKET, SO_KEEPALIVE,
			(const char*) &iSockAttrOn, sizeof(iSockAttrOn)) < 0) {
		perror("keep alive fail\n");
	}
	if (setsockopt(iSockfd, SOL_TCP, TCP_KEEPIDLE, (const char*) &iIdleTime,
			sizeof(iIdleTime)) < 0) {
		perror("keep idle fail");
	}
	if (setsockopt(iSockfd, SOL_TCP, TCP_KEEPINTVL, (const char*) &iInterval,
			sizeof(iInterval)) < 0) {
		perror("keep intvl fail");
	}
	if (setsockopt(iSockfd, SOL_TCP, TCP_KEEPCNT, (const char*) &iCount,
			sizeof(iCount)) < 0) {
		perror("keep cnt fail");
	}
}

int kill_sniffer(void) {
	int ret = 0;
	ret |= system("killall sniffer");
	ret |= system("killall mote");
	return ret;
}

int process_client(CLIENT *client) {
	int readcount;
	PACKET packet;
	packet.recv_fd = client->sockfd;
	packet.recv = socket_recv;
	readcount = packet_recv(&packet);
	if (readcount <= 0) {
		//client close this connection
		printf("%s connection close\n", client->ip);
		close(client->sockfd);
		client->sockfd = -1;
		kill_sniffer();
		return -1;
	} else {
		printf("packet type is %d\n", packet.type);
		switch (packet.type) {

		case CMD_OPEN_SNIFFER: {
			//open sniffer
			int pid;
			signal(SIGCHLD, SIG_IGN);
			pid = fork();
			if (pid < 0) {
				perror("Create thread:");
				exit(1);
			} else if (pid == 0) {
				execlp("/opt/sniffer/bin/sniffer", "/opt/sniffer/bin/sniffer",
						client->ip, (char *) 0);
				_exit(1);
			}
		}
		default:
			break;
		} //switch (packet.type)
	}
	return 0;
}

int accept_client(CLIENT *client, int sockfd) {
	PACKET pkt;
	int newsockfd;
	struct sockaddr_in client_addr;
	int client_addr_len =  sizeof(client_addr);

	newsockfd = accept(sockfd, (struct sockaddr *) &client_addr,
			&client_addr_len);
	if (newsockfd < 0) {
		perror("Accepting failed");
		return -1;
	}

	printf("Connect from %s. socket fd is %d\n", inet_ntoa(client_addr.sin_addr),
			newsockfd);

	if (client->sockfd >= 0)
		close(client->sockfd);

	client->sockfd = newsockfd;
	strcpy(client->ip, inet_ntoa(client_addr.sin_addr));

	heart_beat_check(client->sockfd, 1, 1, 5, 3);

	bzero(&pkt, sizeof(PACKET));
	pkt.type = EV_LISTENER_CONN_SUCCESS;
	pkt.send_fd = client->sockfd;
	pkt.send = socket_send;
	packet_send(&pkt);
	return 0;
}

int main(int argc, char **argv) {
	int sockfd;

	sockfd = socket_server(SERVERPORT, MAXCONNECT);
	if (sockfd < 0) {
		fprintf(stderr, "socket error");
		return -1;
	}

	CLIENT client;
	client.sockfd = -1;

	while (1) {
		int max_fd;
		fd_set readfds;
		struct timeval to;

		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		if (client.sockfd >= 0)
			FD_SET(client.sockfd, &readfds);

		max_fd = sockfd > client.sockfd ? sockfd + 1 : client.sockfd + 1;

		to.tv_sec = 1;
		to.tv_usec = 0;
		if (!select(max_fd, &readfds, NULL, NULL, &to))
			continue;

		if (client.sockfd >= 0 && FD_ISSET(client.sockfd, &readfds)) {
			//client handle data
			process_client(&client);
		} else if (FD_ISSET(sockfd, &readfds)) {
			//new client
			accept_client(&client, sockfd);
		}
	} //while(1)
}

