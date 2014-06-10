/********************************************************
 * Socket Network Transfer Client
 * Compile:	gcc
 * Author:  	grey
 * Last Modify:  		2009.05.19
 *********************************************************/

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>


#define Trace(...);    //do { printf("%s:%u:TRACE: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);
#define Debug(...);    //do { printf("%s:%u:DEBUG: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);
#define Info(...);	do { printf("%s:%u:INFO: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);
#define Error(...);   do { printf("%s:%u:ERROR: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf(" [%s]\n", __FUNCTION__); } while (0);


/***********************************************************
 *	socket_open()
 *		Open one TCP/IP stream socket
 *
 * return:
 *		>0 --> socket number
 *		-1 --> failed
 *
 * parameter:
 *
 ************************************************************/
int socket_open(void) {
	int socket_fd;

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		Debug("ERROR:socket open failed.\n");
		return -1;
	}

	return socket_fd;
}

/***********************************************************
 *	socket_connect_dst()
 *		Connect client socket to server
 *
 * return:
 *		0 --> success
 *		-1 --> failed
 *
 * parameter:
 *		@socket_fd		socket file descriptor
 *		@dst_ip				server IP address
 * 	@dst_port			server listen port number
 *
 ************************************************************/
int socket_connect_dst(int socket_fd, char *dst_ip, int dst_port) {
	struct sockaddr_in dst_addr;

	bzero(&dst_addr, sizeof(struct sockaddr_in));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = htons(dst_port);
	dst_addr.sin_addr.s_addr = inet_addr(dst_ip);
	if (connect(socket_fd, (struct sockaddr *) &dst_addr,
			sizeof(struct sockaddr_in)) == -1) {
		Debug("WARNING:socket connect to %s port %d failed.\n", dst_ip, dst_port);
		return -1;
	}

	Debug("Socket connected to %s port %d.\n", dst_ip, dst_port);
	return 0;
}

/***********************************************************
 *	socket_close()
 *		Close socket of the fd
 *
 * return:
 *
 * parameter:
 *		@socket_fd		socket file descriptor
 *
 ************************************************************/
void socket_close(int socket_fd) {
	Debug("EVENT:socket %d closed.\n", socket_fd);
	if (socket_fd > 0)
		close(socket_fd);
}

/***********************************************************
 *	socket_recv()
 *		Receive data from socket
 *
 * return:
 *		>0 --> Received bytes counter
 *		-1 --> failed
 *
 * parameter:
 *		@socket_fd		socket file descriptor
 *		@buffer				receive data buffer pointer
 * 	@length				receive data buffer max length
 *
 ************************************************************/
int socket_recv(int socket_fd, void *buffer, int length) {
	return recv(socket_fd, buffer, length, 0);
}

/**
 * recv socket with timeout
 * @return: Returns the number read or -1 for errors, -2 for timeout
 */
int socket_recv_timeout(int socket_fd, void *buffer, int length, struct timeval *timeout) {
	int ret;
	fd_set input;

	FD_ZERO(&input);
	FD_SET(socket_fd, &input);

	ret = select(socket_fd + 1, &input, NULL, NULL, timeout);

	// see if there was an error or actual data
	if (ret < 0) {
		perror("select");
	} else if (ret == 0) {
		return -2;
	} else {
		if (FD_ISSET(socket_fd, &input)) {
			return socket_recv(socket_fd, buffer, length);
		}
	}

	return -1;
}

/***********************************************************
 *	socket_send()
 *		Send data by socket
 *
 * return:
 *		>0 --> sent bytes counter
 *		-1 --> failed
 *
 * parameter:
 *		@socket_fd		socket file descriptor
 *		@buffer				send data buffer pointer
 * 	@length				send data length
 *
 ************************************************************/
int socket_send(int socket_fd, void *buffer, int length) {
	int ret;

	ret = send(socket_fd, buffer, length, 0);
	if (ret == -1) {
		Debug("ERROR:socket send data failed.\n");
		perror("send");
		sleep(1);
		ret = send(socket_fd, buffer, length, MSG_DONTWAIT);
		if (ret == -1) {
			Debug("ERROR:socket send data failed.\n");
			perror("MSG_DONTWAIT send");
		}
	}

	return ret;
}

/**
 * become a TCP server, bind to port, listen for maxconnect connections
 * return socket fd, -1 on error
 */
int socket_server(int port, int maxconnect) {
	int ret;
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		Error("socket_server failed");
		return -1;
	}

	int on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	ret = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if (ret < 0) {
		Error("socket_server failed");
		goto err_quit;
	}

	if (listen(sockfd, maxconnect) < 0) {
		Error("socket_server failed");
		goto err_quit;
	}

	return sockfd;

err_quit:
	close(sockfd);
	return -1;
}
