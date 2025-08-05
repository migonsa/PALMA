CC = g++
CFLAGS = -g
TOUCH = touch

OBJS_COMMON = ../common/details.o ../common/addrset.o ../common/packet.o ../common/timer.o ../common/eventloop.o ../common/netitf.o ../common/database.o ../common/config.o

OBJS_CLIENT = main.o palma-client.o states.o config-client.o 

.PHONY: all

all: palma-client

palma-client: $(OBJS_CLIENT) $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o palma-client $(OBJS_CLIENT) $(OBJS_COMMON)

main.o: main.cpp palma-client.h ../common/packet.h config-client.h
	$(CC) $(CFLAGS) -c main.cpp

palma-client.o: palma-client.cpp palma-client.h
	$(CC) $(CFLAGS) -c palma-client.cpp

states.o: states.cpp states.h palma-client.h ../common/details.h
	$(CC) $(CFLAGS) -c states.cpp

config-client.o: config-client.cpp config-client.h ../common/addrset.h
	$(CC) $(CFLAGS) -c config-client.cpp

palma-client.h: config-client.h ../common/database.h ../common/palma.h
	$(TOUCH) palma-client.h

config-client.h: ../common/config.h
	$(TOUCH) config-client.h

states.h: ../common/packet.h ../common/timer.h
	$(TOUCH) states.h

.PHONY: clear

clear:
	rm *.o
