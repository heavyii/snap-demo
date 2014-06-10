#ifndef __PACKET_H__
#define	__PACKET_H__

#define	MAX_FILE_NAME_LENGTH			32
#define	MAX_MSG_BUFFER_LENGTH			220
//#define	MAX_MOTE_NUM				64
#define	MAX_MOTE_NUM					16
#define	MAX_SERIAL_MSG_LENGTH			256
#define MAX_GROUP_DATA          100
#define	MAX_WLP_IDLIST					128

#define SIM_DATA_LENGTH 28

// define packet struct, reference to the d
typedef struct _PACKET {
	__uint8_t type; //cmd, data, state, cc, event,process

	union // packet content
	{
		// state packet
		__uint8_t state;

		//process message packet
		__uint8_t process;

		// complete code packet
		__uint8_t cc;

		// event packet description
		struct {
			__uint64_t time;
		}__attribute__((__packed__)) ev_open_sniffer;

		struct {
			__uint8_t map_length;
			struct {
				__uint8_t usb_num;
				__uint8_t usb_dev;
				__uint8_t msg_port;
				__uint8_t upd_port;
				__uint16_t mote_id;
				__uint8_t mote_amgroup;
				__uint8_t serial_id[9];
			} mote_des[MAX_MOTE_NUM];
		}__attribute__((__packed__)) ev_mote_map;

		struct {
			__uint8_t sniffer_num;
			__uint64_t time;
		}__attribute__((__packed__)) ev_sniffer_conn;

		struct {
			__uint8_t length;
			//__uint8_t		mote_num;//change into type
			//type: used to identify a sink or normal mote
			__uint8_t type;
			__uint8_t usb_dev;
			__uint8_t msg_port;
			__uint8_t upd_port;
			__uint16_t mote_id;
			__uint8_t tos_version;
			__uint8_t serial_id[9];
			__uint8_t port_num[15];
			//
		}__attribute__((__packed__)) ev_mote_conn;

		struct {
			__uint8_t ipaddr[16];
		}__attribute__((__packed__)) ev_listener_conn_fail;

		// data packet message
		struct {
			__uint8_t length;
			__uint64_t absolute_time;
			__uint8_t msg[MAX_MSG_BUFFER_LENGTH];
		}__attribute__((__packed__)) data_msg;

		struct {
			__uint8_t type;
			__uint8_t length;
			__uint8_t serialdata[MAX_SERIAL_MSG_LENGTH];
		}__attribute__((__packed__)) serial_data;

		struct {
			__uint8_t mote_num;
		}__attribute__((__packed__)) cmd_open_mote;

		struct {
			__uint8_t length;
			__uint8_t action;
			__uint8_t port_num[15];
		}__attribute__((__packed__)) cmd_control_usbport;


		struct {
			__uint8_t cmd_cc;
			__uint8_t cmd_type;
		}__attribute__((__packed__)) cmd_complete_code;

	}__attribute__((__packed__)) content;

	int send_fd; //fix it
	int (*send)(int fd, void *buffer, int length);
	int recv_fd;
	int (*recv)(int fd, void *buffer, int length);

} PACKET;

//end  simulator

#define CMD_TEST				0xFF
// define command code
#define	 CMD_OPEN_MOTE			0x80
#define CMD_OPEN_SNIFFER		0x81
#define CMD_CLOSE_SNIFFER		0x82

#define	CMD_START_SAMPLING				0x70
#define	CMD_STOP_SAMPLING					0x71

//mote CMD
#define	CMD_GET_STATE							0x76
#define CMD_SEND_SERIALDATA 			0x79        //command of sending serial data
#define CMD_HEART_BEAT			0x00

// define data code
#define USER_CUSTOM_DATA	0x90
#define PRINTF_DATA			0x91
#define SERIAL_DATA			0x93

#define	EV_SNIFFER_CONN					0x61
#define	EV_MOTE_CONN					0x62
#define EV_LISTENER_CONN_SUCCESS		0x63
#define EV_LISTENER_CONN_FAILED			0x64
#define	COMPLETE_CODE					0xaa

// define	complete code
#define	CC_SUCCESS					0x0
#define	CC_FAIL							0x1
#define	CC_SKIP							0x2

//complete code result
enum cc_result {
	RES_DEFAULT = 0x0,
	RES_NEED_SIM_CONFIG = 0x1,
	RES_GET_SIM_PROGRAM = 0x2,
	RES_GET_SIM_CONFIG = 0x3,
	RES_CRTL_SIM = 0x4
};

#define	STATE_CODE					0x55
#define PROCESS_CODE                0X56

int
packet_recv(PACKET *packet);

int
packet_send(PACKET *packet);

void packet_print(PACKET *packet);

#endif /* __PACKET_H__ */

