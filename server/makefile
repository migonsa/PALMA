CC = g++
CFLAGS = -g
TOUCH = touch

OBJS_COMMON = ../common/details.o ../common/addrset.o ../common/packet.o ../common/timer.o ../common/eventloop.o ../common/netitf.o ../common/database.o ../common/siphash.o ../common/config.o

OBJS_SERVER = main.o palma-server.o config-server.o

.PHONY: all

all: palma-server

palma-server: $(OBJS_SERVER) $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o palma-server $(OBJS_SERVER) $(OBJS_COMMON)

main.o: main.cpp palma-server.h config-server.h ../common/packet.h 
	$(CC) $(CFLAGS) -c main.cpp

palma-server.o: palma-server.cpp palma-server.h ../common/details.h
	$(CC) $(CFLAGS) -c palma-server.cpp

config-server.o: config-server.cpp config-server.h ../common/addrset.h
	$(CC) $(CFLAGS) -c config-server.cpp

palma-server.h: config-server.h ../common/netitf.h ../common/eventloop.h ../common/database.h ../common/siphash.h ../common/palma.h
	$(TOUCH) palma-server.h

config-server.h: ../common/config.h
	$(TOUCH) config-server.h

.PHONY: clear

clear:
	rm *.o
