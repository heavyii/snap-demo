#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>
#include "misc.h"


/**
 * print hex stream
 */
void printhex(const void *buf, int len) {
	int i = 0;
	const char *p = (const char *) buf;
	for (i = 0; i < len; i++) {
		printf(" %02hhx", *p++);
	}
	putchar('\n');
}

/**
 * dump hex stread with message
 */
void dump(const char *msg, const void *buf, int len)
{
  printf("%s", msg);
  printhex(buf, len);
}
