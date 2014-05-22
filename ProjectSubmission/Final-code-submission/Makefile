# Makefile for TCPD m1 and m2

CC = /usr/local/bin/gcc
OBJM1 = tcpd_m1.c 
OBJM2 = tcpd_m2.c
OUTM1 = tcpd_m1
OUTM2 = tcpd_m2
OBJC = ftpc.c 
OBJS = ftps.c
OUTC = ftpc
OUTS = ftps
OBJT = timer.c
OUTT = timer
HDR = tcpd_socket.h
OBJTS = libtcpd_socket.c
OUTTS = libtcpd_socket.so
CFLAGS = -g -Wall 
# setup for system
LIBS =  -lxnet

all:	$(OUTM1) $(OUTM2) $(OUTC) $(OUTS) $(OUTT) 

$(OUTM1):	$(OBJM1)
	$(CC) $(CFLAGS) -o $@ $(OBJM1) $(LIBS)

$(OUTM2):	$(OBJM2)
	$(CC) $(CFLAGS) -o $@ $(OBJM2) $(LIBS)

$(OUTC):	$(OBJC) $(HDR) $(OUTTS)
	$(CC) $(CFLAGS) -o $@ $(OBJC) $(LIBS) -L. -ltcpd_socket
	mkdir -p ./client
	cp $(OUTC) ./client

$(OUTT):	$(OBJT)
	$(CC) $(CFLAGS) -o $@ $(OBJT) $(LIBS)

$(OUTS):	$(OBJS) $(HDR) $(OUTTS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)  -L. -ltcpd_socket
	mkdir -p ./server
	cp $(OUTS) ./server

$(OUTTS):	$(OBJTS)
	 $(CC) $(CFLAGS) -c -fPIC $(OBJTS) -o $@

clean:	
	rm -f $(OUTM1) $(OUTM2) $(OUTC) $(OUTS) $(OUTT) $(OUTTS)
