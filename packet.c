#include <sys/types.h>
#include <openssl/aes.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "misc.h"
#include "hdlc.h"
#include "packet.h"

#define DBG(x)

static int packet_length(PACKET *packet) {
	int length = 0;

	switch (packet->type) {
	case CMD_OPEN_MOTE:
		length = sizeof(packet->content.cmd_open_mote)
				+ ((void *) &packet->content - (void *) packet);
		break;

	case CMD_OPEN_SNIFFER:
		length = sizeof(packet->content.ev_open_sniffer)
				+ ((void *) &packet->content - (void *) packet);
		break;

	case CMD_CLOSE_SNIFFER:
	case CMD_START_SAMPLING:
	case CMD_STOP_SAMPLING:

	case EV_LISTENER_CONN_SUCCESS:
		length = ((void *) &packet->content - (void *) packet);
		break;
	case CMD_SEND_SERIALDATA:
		length = packet->content.serial_data.length
				+ ((void *) &packet->content.serial_data.serialdata
						- (void *) packet);
		break;
	case USER_CUSTOM_DATA:
	case PRINTF_DATA:
	case SERIAL_DATA:
		length = packet->content.data_msg.length
				+ ((void *) &packet->content.data_msg.msg - (void *) packet);
		break;
	case EV_LISTENER_CONN_FAILED: {
		length = sizeof(packet->type)
				+ sizeof(packet->content.ev_listener_conn_fail.ipaddr);
	}
		break;

	case EV_SNIFFER_CONN:
		length = sizeof(packet->content.ev_sniffer_conn)
				+ ((void *) &packet->content - (void *) packet);
		break;

	case EV_MOTE_CONN:
		length = sizeof(packet->content.ev_mote_conn)
				+ ((void *) &packet->content - (void *) packet) - 15
				+ packet->content.ev_mote_conn.length;
		break;

	case STATE_CODE:
		length = sizeof(packet->content.state)
				+ ((void *) &packet->content - (void *) packet);
		break;

	case PROCESS_CODE:
		length = sizeof(packet->content.process)
				+ ((void *) &packet->content - (void *) packet);
		break;

	case COMPLETE_CODE:
		//length = sizeof(packet->content.cc) + ((void *)&packet->content - (void *)packet);
		length = sizeof(packet->content.cmd_complete_code)
				+ ((void *) &packet->content - (void *) packet);
		break;

	case CMD_HEART_BEAT:
		length = 1;
		break;

	default:
		break;
	} //end switch(packet->type)

	//DBG(("packet length = %d.\n", length));
	return length;
}

int packet_recv(PACKET *packet) {
	HDLC hdlc;
	__uint8_t recv_buffer[2048] = { 0 }; //socket data receive buffer
	int recv_length;
	int length;
	int i;

	if (packet->recv == NULL) // packet recv() not define
			{
		DBG(("packet recv() not define.\n"));
		return -1;
	}

	hdlc_init_recv(&hdlc, (__uint8_t *) packet, sizeof(PACKET));

	recv_length = packet->recv(packet->recv_fd, recv_buffer, 1024);
	if (recv_length == -1) {
		DBG(("ERROR:data receive failed.\n"));
		return -1;
	}

	for (i = 0; i < recv_length; ++i) {
		length = hdlc_recv_char(&hdlc, recv_buffer[i]);
		if (length > 0) {
			return packet_length(packet);
		} else if (length == -1) {
			DBG(("ERROR:packet data overflow"));
			return -1;
		}
	} // end for

	return 0;
}

int packet_send(PACKET *packet) {
	int length = 0;
	unsigned char send_buffer[2048];
	if (packet->send == NULL) {
		DBG(("packet send() not define.\n"));
		return -1;
	}

	length = packet_length(packet);

	if (length == 0)
		return 0;

	length = hdlc_send_buffer(send_buffer, (__uint8_t *) packet, length);
	int ret = packet->send(packet->send_fd, send_buffer, length);

	return ret;
}

/**
 * print hex stream of a packet, for debug.
 */
void packet_print(PACKET *packet) {
	int len;
	len = packet_length(packet);
	printf("packet->type: %2hhx\npacket length: %d\n", packet->type, len);
	dump("packet", packet, len);
}
