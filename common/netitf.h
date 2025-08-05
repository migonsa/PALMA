#ifndef NETITF_H
#define NETITF_H

#include <linux/if_packet.h>
#include <stdint.h>
#include "eventloop.h"
#include "packet.h"

class Palma;

class NetItf : public EventSource
{
	int m_ifidx;
	Palma *m_protocol;
	
public:
	NetItf(Palma *protocol);
	~NetItf();
	void init(uint8_t *ifname);
	int onInput();
	void netsend(Packet *pkt);
	void fillMreq(packet_mreq& mreq, uint64_t addr, bool multicast);
	void addAddr(uint64_t addr, bool multicast = false);
	void delAddr(uint64_t addr, bool multicast = false);
};

#endif
