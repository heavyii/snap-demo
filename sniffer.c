/*
 * this simple program demostrate how to send user data  to SNAP using hdlc protocol.
 * 
 */

#include <string.h>
#include <stdlib.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <usb.h>
#include "packet.h"
#include "hdlc.h"
#include "socket.h"
#include "misc.h"



static int sniffer_connect(char *server_ip, int server_port) {
	int socket_fd;
	// socket open
	if ((socket_fd = socket_open()) == -1)
		return -1;

	// socket connect to server
	if (socket_connect_dst(socket_fd, server_ip, server_port)
			!= 0) {
		close(socket_fd);
		return -1;
	}

	printf("Socket connected to %s port %d.\n", server_ip, server_port);
	return socket_fd;
}

static int sniffer_ev_connect(int sock_fd) {
	PACKET packet;


	packet.type = EV_SNIFFER_CONN;
	packet.content.ev_sniffer_conn.sniffer_num = 0;
	packet.content.ev_sniffer_conn.time = 0;
	packet.send_fd = sock_fd;
	packet.send = socket_send;

	return packet_send(&packet);
}

int process_sniffer(int  sockfd) {
	int readcount;
	PACKET packet;
	packet.recv_fd = sockfd;
	packet.recv = socket_recv;
	readcount = packet_recv(&packet);
	if (readcount <= 0) {
		//client close this connection
		printf("connection close %d\n", sockfd);
		close(sockfd);
		sockfd = -1;
		exit(0);
	} else {
		printf("packet type is %d\n", packet.type);
		switch (packet.type) {

		default:
			break;
		} //switch (packet.type)
	}
	return 0;
}

int main(int argc, char **argv) {
	int sock_fd;
	char *server_ip = argv[1];

	puts("sinffer start...");

	sock_fd = sniffer_connect(server_ip, 5000);
	if (sock_fd < 0) {
		fprintf(stderr, "sniffer failed, exit...\n");
		close(sock_fd);
		return 1;
	}
	puts("sinffer connect...");
	sniffer_ev_connect(sock_fd);
	puts("sinffer env connect done");

	//TODO: start mote

	while(1) {
		fd_set readfds;
		struct timeval to;

		FD_ZERO(&readfds);
		FD_SET(sock_fd, &readfds);

		to.tv_sec = 1;
		to.tv_usec = 0;
		if (!select(sock_fd + 1, &readfds, NULL, NULL, &to))
			continue;

		if (FD_ISSET(sock_fd, &readfds)) {
			//handle server data

		}
	}

	close(sock_fd);
	return 0;
}

