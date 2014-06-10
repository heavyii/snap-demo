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
#include "socket.h"
#include "com_port.h"
#include "hdlc.h"
#include "packet.h"
#include "misc.h"
#include "crc.h"

#define	 	VERSION		"mote-1.4"

#define DBG(x)

#define	HELP_INFO()		printf("Usage: mote [OPTION]... \n\n");		\
				printf("\t-m [m]sg_port\n\t\tSerial port file for message sampling. /dev/ttyUSB* are valid, default is /dev/ttyUSB0.\n\n");		\
				printf("\t-b msg_port_[b]aud\n\t\tSerial baud for message sampling, 2400,4800,9600,38400,56700,115200 are valid, default is 57600.\n\n");		\
				printf("\t-u [u]pd_port\n\t\tSerial port file for uploading mote. /dev/ttyUSB* are valid, default is /dev/ttyUSB0.\n\n");		\
				printf("\t-U [U]pd_dir\n\t\tDirectory path for upload files, default is /opt/sniffer/upd/ .\n\n");		\
				printf("\t-B [B]in_dir\n\t\tDirectory path for .sh and exe files, default is /opt/sniffer/bin/ .\n\n");		\
				printf("\t-i server_[i]p\n\t\tServer IP address of data server. Format xxx.xxx.xxx.xxx is valid, default is 192.168.14.252 .\n\n");		\
				printf("\t-p server_[p]ort\n\t\tServer listen port of data server, like 10000, default is 5001.\n\n");		\
				printf("\t-t mote_[t]ype\n\t\tMote type option, micaz and telosb are valid, default is telosb.\n\n");		\
				printf("\t-V tinyos_[V]ersion\n\t\tMote program of tinyos verison, tos1(tinyos-1.x) and tos2(tinyos-2.x) are valid, default is tos1.\n\n");		\
				printf("\t-n sniffer_[n]umber\n\t\tThe sniffer number of this mote.\n\n");		\
				printf("\t-N usb_[N]umber\n\t\tThe usb device number of this mote in the mote-map.\n\n");		\
				printf("\t-v\n\t\tShow version.\n\n");		\
				printf("\t-h\n\t\tShow help.\n\n");			\
				printf("Notes: Mostly, it is used by sniffer process. It also can be used independently. Before use it, make sure the listen port of the server is open. \n\n");		\

#define MOTE_TYPE_NORMAL 0
#define MOTE_TYPE_SINK   1

// define mote control struct
typedef struct _MOTE {
	// mote attribute
	char *sniffer_num;
	char *usb_num;
	char *msg_port;
	int msg_port_baud;
	char *upd_port;
	char *upd_dir;
	char *bin_dir;
	char *server_ip;
	int server_port;
	char mote_id[6];
	char change_id[5];
	char tos_version[4];
	char *mote_type;
	char *serial_num;
	char *port_num;
	char bfirstReadId;
	/* type: to identify mote type: is sink or usual mote
	 * Value:
	 * 		0:  a normal mote
	 * 		1:  a sink mote
	 */
	char type;
	//for lock pid file
	int sink_pid_fd;

	// socket attribute
	int socket_fd;
	HDLC socket_hdlc;

	// sampling config
	int com_fd;
	HDLC com_hdlc;
	time_t start_sampling_time_sec;
	suseconds_t start_sampling_time_usec;
	char is_start_sampling;

	// clear buffer
	char is_clear_buffer;
	time_t start_clear_time_sec;
	suseconds_t start_clear_time_usec;

	time_t ref_time_sec; //fix it
	suseconds_t ref_time_msec;

	// logic control
	char state;
	char energy_state;
	int child_pid; // child process id, include upload and ftp
	int energy_measure_pid; // child pid, used to measure the energy

} MOTE;

// define	mote state
#define	ST_MOTE_IDLE						0
#define	ST_MOTE_SAMPLING				1

#define	TIMEOUT_SEC(len,baud)	(len * 20 / baud + 2)
#define	TIMEOUT_USEC	0

#define	MAX(X,Y)	(((X) > (Y)) ? (X) : (Y))

static MOTE global_mote;

/* Slow implementation of crc function */
static uint16_t crc_byte(uint16_t crc, uint8_t b) {
	uint8_t i;
	//printf("crc is %d",crc);
	crc = crc ^ b << 8;
	i = 8;
	do
		if (crc & 0x8000)
			crc = crc << 1 ^ 0x1021;
		else
			crc = crc << 1;
	while (--i);
	//printf(" the b is%d, and the crc is %d\n",b,crc);

	return crc;
}

static uint16_t crc_packet(uint8_t *data, int len) {
	uint16_t crc = 0;

	while (len-- > 0)
		crc = crc_byte(crc, *data++);

	return crc;
}

static int mote_set_baud(MOTE *mote) {
	int ret = 0;
	if (!memcmp(mote->mote_type, "telosb", strlen(mote->mote_type))) {
		mote->msg_port_baud = 115200;
	} else if (!memcmp(mote->mote_type, "converter", strlen(mote->mote_type))) {
		mote->msg_port_baud = 57600;
	} else {
		ret = -1;
	}
	return ret;
}

static int mote_state(MOTE *mote) {
	PACKET packet;

	packet.type = STATE_CODE;
	packet.content.state = mote->state;
	packet.send_fd = mote->socket_fd;
	packet.send = socket_send;
	return packet_send(&packet);
}

static int mote_open(MOTE *mote, int argc, char **argv) {
	int ch;
	// set mote default parameter
	mote->msg_port = "/dev/ttyUSB0";
	mote->msg_port_baud = 115200;
	mote->upd_port = "/dev/ttyUSB0";
	mote->upd_dir = "/opt/sniffer/upd/";
	mote->bin_dir = "/opt/sniffer/bin/";
	mote->server_ip = "192.168.14.252";
	mote->server_port = 5001;
	mote->mote_type = "telosb";
	mote->sniffer_num = "0";
	mote->usb_num = "2";
	mote->serial_num = "TB000031S";
	mote->port_num = "0-0";
	//a normal mote by default, not a sink
	mote->type = 0;
	mote->sink_pid_fd = -1;
	mote->bfirstReadId = 1;
	// get command line option
	while ((ch = getopt(argc, argv, ":m:b:u:U:B:i:p:P:t:V:n:N:S:I:hv")) != -1) {
		switch (ch) {
		case 'n': //sniffer_[n]umber
			mote->sniffer_num = optarg;
			break;
		case 'N': //usb_[N]umber
			mote->usb_num = optarg;
			break;
		case 'm': //[m]sg_port
			mote->msg_port = optarg;
			break;
		case 'b': //[b]aud
			mote->msg_port_baud = atoi(optarg);
			break;
		case 'u': //[u]pd_port
			mote->upd_port = optarg;
			break;
		case 'U': //[U]pd_dir
			mote->upd_dir = optarg;
			break;
		case 'B': //[B]in_dir
			mote->bin_dir = optarg;
			break;
		case 'i': //server_[i]p
			mote->server_ip = optarg;
			break;
		case 'p': //server_[p]ort
			mote->server_port = atoi(optarg);
			break;
		case 't': //mote_[t]ype
			mote->mote_type = optarg;
			break;
		case 'P':
			mote->port_num = optarg;
			break;
		case 'S': //[S]erial_num
			mote->serial_num = optarg;
			break;
		case 'I': //[I]d mote id
			strncpy(mote->mote_id, optarg, 6);
			break;
		case 'v': //[v]ersion
			//mote->tos_version[0] = atoi(optarg);
			//printf("Version:%s\n", VERSION);
			//printf("Version:%s\n", optarg);
			exit(0);
		case '?':
			DBG(("WARNING:unknow option: \"-%c\"\n", optopt));
			HELP_INFO()
			;
			exit(0);
		case ':':
			DBG(("WARNING:option \"-%c\" need argument.\n", optopt));
			HELP_INFO()
			exit(0);
		case 'h': //[h]elp
		default:
			HELP_INFO()
			;
			exit(0);
		} // end switch
	} // end while

	mote_set_baud(mote);

	mote->bfirstReadId = 0;

	mote->type = MOTE_TYPE_NORMAL;

	// init mote attribute
	mote->socket_fd = -1;
	mote->com_fd = -1;
	mote->state = ST_MOTE_IDLE;

	mote->start_sampling_time_sec = 0;
	mote->start_sampling_time_usec = 0;
	mote->is_start_sampling = 1;
	mote->is_clear_buffer = 1;
	mote->child_pid = -1;
	mote->energy_measure_pid = -1;

	return 0;
}

static int mote_connect(MOTE *mote) {
	// socket open
	if ((mote->socket_fd = socket_open()) == -1)
		return -1;

	// socket connect to server
	if (socket_connect_dst(mote->socket_fd, mote->server_ip, mote->server_port)
			!= 0)
		return -1;

	return 0;
}

static int mote_process_sampling_data(MOTE *mote, unsigned char *buffer,
		int len) {
	int i;
	__uint64_t absolute_time = 621355968000000000LL;

	struct timeval tv;
	struct timezone tz;

	PACKET packet;
	bzero(&packet, sizeof(packet));

	if (len > 7) {
		// get current time
		gettimeofday(&tv, &tz);

		absolute_time += tv.tv_sec * 10000000LL;
		absolute_time += tv.tv_usec * 10LL;
		//printf("data is ready...\n");
		//serial printf data
		if (buffer[8] == 0x64) {
			//printf("\nPRINTF_DATA\n");
			for (i = 9; i < len; i++) {
				if (buffer[i] == 0) {
					packet.content.data_msg.msg[i - 9] = ' ';
					continue;
				}
				packet.content.data_msg.msg[i - 9] = buffer[i];
			}
			// make data packet

			len -= 9;
			packet.type = PRINTF_DATA;
		}
		//mote's normal radio data

		else if (buffer[8] == 0x65) {
			/* nUSER_CUSTOM_DATA */

			packet.content.data_msg.msg[0] = buffer[0];
			packet.content.data_msg.msg[1] = buffer[6];
			for (i = 9; i < len; i++) {
				packet.content.data_msg.msg[i - 7] = buffer[i];
			}
			//packet.content.data_msg.msg[i] = '\0';
			// make data packet
			len -= 7;
			packet.type = USER_CUSTOM_DATA;
		}
		 else {
			/* USER_SERIAL_DATA */

			for (i = 9; i < len; i++) {
				packet.content.data_msg.msg[i - 9] = buffer[i];
			}
			// make data packet
			len -= 9;
			packet.type = SERIAL_DATA;
		}

		//packet.content.data_msg.length = (__uint8_t)(((void *)packet.content.data_msg.msg - (void *)&packet.content.data_msg.ref_time_sec) + len - 1);
		packet.content.data_msg.length = (__uint8_t ) len;
		packet.content.data_msg.absolute_time = absolute_time;
		//packet.content.data_msg.ref_time_sec = (__uint32_t)(curr_time_sec - mote->ref_time_sec);
		//packet.content.data_msg.ref_time_msec = (__uint16_t)((curr_time_usec - mote->ref_time_msec) / 1000);

		packet.send_fd = mote->socket_fd;
		packet.send = socket_send;
		if (packet_send(&packet) == -1) {
			DBG(("\nERROR: socket send failed...\n"));
			return -1;
		}
	} //end if (len > 7)

	return 0;
}

/**
 * NOTE: DO NOT change buffer and mote->com_hdlc if you keep sampling, buffer may
 * contains last uncompleted data. If you change it, we may loss packet!!!
 */
static int mote_process_sampling(MOTE *mote, unsigned char *buffer, int length) {
	int data_len = 0;
	unsigned char buf[2048];
	unsigned char *p = NULL;
	data_len = read(mote->com_fd, buf, sizeof(buf));
	if (data_len <= 0) {
		return 0;
	}

	if (mote->com_hdlc.recv_buffer != buffer
			|| mote->com_hdlc.recv_buffer_length != length)
		hdlc_init_recv(&mote->com_hdlc, buffer, length);

	/* handle all serial data read */
	p = buf;
	while (data_len-- > 0) {
		int len;
		len = hdlc_recv_char(&mote->com_hdlc, *p++);
		if (len > 0) {
			/* one HDLC packet */

			/* check CRC */
			uint16_t readcrc, compute_crc;
			compute_crc = crc_packet(buffer, len - 2);
			readcrc = buffer[len - 2] | buffer[len - 1] << 8;
			if (compute_crc != readcrc) {
				fprintf(stderr, "CRC error!!\n");
				continue;
			}

			if (mote_process_sampling_data(mote, buffer, len) != 0)
				return -1;
		}
	}
	return 0;
}

static int mote_cmd_start_sampling(MOTE *mote, unsigned char *buf, int len) {
	int fd;
	printf("Start sampling port (%s) baud (%d)\n", mote->msg_port,
			mote->msg_port_baud);
	fd = com_open(mote->msg_port);
	if (fd < 0)
		return -1;

	if (com_set(fd, mote->msg_port_baud, 8, 1, 'n', 'n') != 0) {
		return -1;
	}
	mote->com_fd = fd;

	/* init hdlc buffer to prepare for a new sampling */
	hdlc_init_recv(&mote->com_hdlc, buf, len);

	// change state
	mote->state = ST_MOTE_SAMPLING;
	mote_state(mote);
	return 0;
}

static int mote_cmd_stop_sampling(MOTE *mote) {
	// close serial
	com_close(mote->com_fd);
	mote->com_fd = -1;

	// stop sampling state
	mote->state = ST_MOTE_IDLE;
	mote_state(mote);

	return 0;
}

static int mote_ev_connect(MOTE *mote) {
	PACKET packet;
	struct timeval tv;
	struct timezone tz;

	packet.type = EV_MOTE_CONN;
	packet.content.ev_mote_conn.usb_dev = 2;

	packet.content.ev_mote_conn.type = 0;
	packet.content.ev_mote_conn.msg_port = (__uint8_t ) atoi(
			mote->msg_port + 11);
	packet.content.ev_mote_conn.upd_port = (__uint8_t ) atoi(
			mote->upd_port + 11);
	packet.content.ev_mote_conn.mote_id = (__uint16_t ) atoi(mote->mote_id);
	packet.content.ev_mote_conn.tos_version = (__uint8_t ) atoi(
			mote->tos_version);
	memcpy(packet.content.ev_mote_conn.serial_id, mote->serial_num,
			strlen(mote->serial_num));
	memcpy(packet.content.ev_mote_conn.port_num, mote->port_num,
			strlen(mote->port_num));
	packet.content.ev_mote_conn.port_num[strlen(mote->port_num)] = 0;
	packet.content.ev_mote_conn.length = strlen(mote->port_num);

	gettimeofday(&tv, &tz);
	mote->ref_time_sec = tv.tv_sec;
	mote->ref_time_msec = tv.tv_usec;

	packet.send_fd = mote->socket_fd;
	packet.send = socket_send;

	return packet_send(&packet);
}

static int mote_cc(MOTE *mote, __uint8_t cc, __uint8_t type) {
	PACKET packet;

	packet.type = COMPLETE_CODE;
	//packet.content.cc = cc;
	packet.content.cmd_complete_code.cmd_cc = cc;
	packet.content.cmd_complete_code.cmd_type = type;
	packet.send_fd = mote->socket_fd;
	packet.send = socket_send;

	return packet_send(&packet);
}

static int mote_process(MOTE *mote) {
	// select control
	int fs_sel;
	fd_set fs_read;
	int max_fd;
	struct timeval to;

	// command control
	int cmd_ret;
	unsigned char complete;
	unsigned char msg_buffer[MAX_SERIAL_MSG_LENGTH];
	memset(msg_buffer, 0, MAX_SERIAL_MSG_LENGTH);
	PACKET packet;
	memset((unsigned char*) &packet, 0, sizeof(PACKET));

	if (mote == NULL) {
		DBG(("ERROR:Invalid pointer.\n"));
		return -1;
	}

	DBG(("mote_ev_connect is ready\n"));
	if (mote_ev_connect(mote) == -1)
		return -1;

	// init serial message receive buffer
	hdlc_init_recv(&mote->com_hdlc, msg_buffer, sizeof(msg_buffer));

	packet.recv_fd = mote->socket_fd;
	packet.recv = socket_recv;

	while (1) {
		// config select system call
		FD_ZERO(&fs_read);
		if (mote->socket_fd > 0)
			FD_SET(mote->socket_fd, &fs_read);
		if (mote->com_fd > 0)
			FD_SET(mote->com_fd, &fs_read);
		max_fd = MAX(mote->socket_fd, mote->com_fd);
		to.tv_sec = 1;
		to.tv_usec = 0;

//		DBG(("start select...\n"));
		// select socket and serial input without timeout.
		fs_sel = select(max_fd + 1, &fs_read, NULL, NULL, &to);
		if (fs_sel) {
			// socket received data
			if (FD_ISSET(mote->socket_fd, &fs_read)) {
				printf("socket received data\n");
				int p_ret = 0;
				if ((p_ret = packet_recv(&packet)) > 0) {

					switch (packet.type) {
					case CMD_GET_STATE: {
						if (mote_state(mote) == -1)
							return -1;
						break;
					}

					case CMD_START_SAMPLING: {
						if (mote->state == ST_MOTE_IDLE) {
							cmd_ret = mote_cmd_start_sampling(mote, msg_buffer,
									sizeof(msg_buffer));
							complete = (cmd_ret == 0) ? CC_SUCCESS : CC_FAIL;
							if (mote_cc(mote, complete, packet.type) == -1)
								return -1;
						} // end if ST_MOTE_IDLE
						else {
							if (mote_cc(mote, CC_SKIP, packet.type) == -1)
								return -1;
						}
						break;
					}

					case CMD_STOP_SAMPLING: {
						if (mote->state == ST_MOTE_SAMPLING) {
							cmd_ret = mote_cmd_stop_sampling(mote);
							complete = (cmd_ret == 0) ? CC_SUCCESS : CC_FAIL;
							if (mote_cc(mote, complete, packet.type) == -1)
								return -1;
						} else {
							if (mote_cc(mote, CC_SKIP, packet.type) == -1)
								return -1;
						}
						break;
					}

					default:
						puts("WARNING:ignore other packets");
						break;
					} //end switch packet type
				} //end if (packet_recv(&packet) > 0)
				else if (p_ret == 0) {
					fprintf(stderr, "Rec Error Packet");
				} else {
					fprintf(stderr, "Server shutdown this client");
					return -1;
				}
			} //end select socket

			// serial received data
			if (mote->state == ST_MOTE_SAMPLING
					&& FD_ISSET(mote->com_fd, &fs_read)) {
				if (mote_process_sampling(mote, msg_buffer, sizeof(msg_buffer))
						!= 0) {
					fprintf(stderr, "ERROR: Process sampling failed.\n");
					return -1;
				}

			} //end select serial received data

		} //end if (fs_sel)
		else if (fs_sel == 0) {
			//timeout
		} else {
			DBG(("ERROR: select() return %d\n", fs_sel));
			return -1;
		}
	} //end while(1)

	return 0;
}

static int mote_close(MOTE *mote) {

	// kill child process
	if (mote->child_pid != -1 && kill(mote->child_pid, SIGKILL) == -1) {
		DBG(("kill upload process failed.\n"));
		return -1;
	}
	mote->child_pid = -1;

	com_close(mote->com_fd);
	mote->com_fd = -1;

	if (mote->socket_fd >= 0) {
		close(mote->socket_fd);
		mote->socket_fd = -1;
	}

	return 0;
}

int main(int argc, char **argv) {
	MOTE *mote = &global_mote;
	bzero(mote, sizeof(MOTE));

	/* parser process argument */
	if (mote_open(mote, argc, argv) != 0) {
		fprintf(stderr, "mote open failed, mote exit...\n");
		return 1;
	}
	/* mote connect to server */
	if (mote_connect(mote) != 0) {
		fprintf(stderr, "mote_connect failed, mote exit...\n");
		mote_close(mote);
		return 1;
	}
	/* mote receive and process command */
	if (mote_process(mote) != 0) {
		fprintf(stderr, "mote_process failed, mote exit...\n");
		mote_close(mote);
		return 1;
	}

	if (mote_close(mote) != 0) {
		fprintf(stderr, "mote close failed, mote exit...\n");
		return 1;
	}

	fprintf(stderr, "mote main return 0, mote exit...\n");
	return 0;
}

