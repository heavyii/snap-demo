#
# file: $(TOPDIR)/Makefile
#

#CROSS_COMPILE ?= arm-openwrt-linux-uclibcgnueabi-
#CROSS_COMPILE ?= arm-linux-
CC=$(CROSS_COMPILE)gcc

CFLAGS := -O1 -Wall -pipe

APP = mote sniffer listener
	

all: $(APP)

.c.o:
	@$(CC) -c $(CFLAGS) $(INCLUDES)   $< -o $@

mote_obj = mote.o com_port.o hdlc.o misc.o packet.o socket.o crc.o
mote: $(mote_obj)
	$(CC) $^ -o $@  

sniffer_obj = sniffer.o	com_port.o hdlc.o misc.o packet.o socket.o
sniffer: $(sniffer_obj)
	$(CC) $^ -o $@
	
listener_obj = listener.o com_port.o hdlc.o misc.o packet.o socket.o
listener: $(listener_obj)
	$(CC) $^ -o $@ 
	
clean:
	@rm -rf *.o $(APP)
	

