CC = g++
CFLAGS = -g
TOUCH = touch
MAKE = make

OBJS_COMMON = ../common/details.o ../common/addrset.o ../common/packet.o ../common/timer.o ../common/eventloop.o ../common/netitf.o ../common/database.o ../common/siphash.o

OBJS_CLIENT = main.o palma-server.o config-server.o

.PHONY: all

all: common palma-client palma-server

.PHONY: common

common:
	$(MAKE) -C common all


.PHONY: palma-client

palma-client:
	$(MAKE) -C client all


.PHONY: palma-server

palma-server:
	$(MAKE) -C server all


.PHONY: clear

clear:
	$(MAKE) -C common clear
	$(MAKE) -C client clear
	$(MAKE) -C server clear
