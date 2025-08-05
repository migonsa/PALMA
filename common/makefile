CC = g++
CFLAGS = -g
TOUCH = touch

OBJS_COMMON = details.o addrset.o packet.o timer.o eventloop.o netitf.o database.o siphash.o config.o

.PHONY: all

all: $(OBJS_COMMON)

details.o: details.cpp details.h
	$(CC) $(CFLAGS) -c details.cpp

addrset.o: addrset.cpp addrset.h
	$(CC) $(CFLAGS) -c addrset.cpp

packet.o: packet.cpp packet.h details.h
	$(CC) $(CFLAGS) -c packet.cpp

timer.o: timer.cpp timer.h
	$(CC) $(CFLAGS) -c timer.cpp

eventloop.o: eventloop.cpp timer.h eventloop.h 
	$(CC) $(CFLAGS) -c eventloop.cpp

netitf.o: netitf.cpp netitf.h timer.h eventloop.h packet.h palma.h
	$(CC) $(CFLAGS) -c netitf.cpp

database.o: database.cpp database.h palma.h
	$(CC) $(CFLAGS) -c database.cpp

siphash.o: siphash.cpp siphash.h
	$(CC) $(CFLAGS) -c siphash.cpp

config.o: config.cpp config.h
	$(CC) $(CFLAGS) -c config.cpp

packet.h: addrset.h
	$(TOUCH) packet.h

netitf.h: eventloop.h packet.h
	$(TOUCH) netitf.h

details.h: addrset.h
	$(TOUCH) details.h

palma.h: netitf.h eventloop.h
	$(TOUCH) palma.h

database.h: addrset.h timer.h
	$(TOUCH) database.h

.PHONY: clear

clear:
	rm *.o
