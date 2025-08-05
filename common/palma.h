#ifndef PALMA_H
#define PALMA_H

#include "netitf.h" 
#include "eventloop.h"
#include "timer.h"

class Palma : public ExitHandler
{
public:
	NetItf m_netitf;
	EventLoop m_event_loop;

	Palma()	:	m_netitf(this) {}
	virtual void handlePacket(Packet *pkt) {}
	
	virtual void onExit() {}
	
	static void initRandom()
	{
		Time now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		srand48(now.tv_nsec);
	}
};

#endif
