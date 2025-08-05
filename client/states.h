#ifndef STATES_H
#define STATES_H

#include <string.h>
#include "../common/packet.h"
#include "../common/timer.h"

class PalmaClient;

class State
{
public:
	PalmaClient *m_protocol;

	State(PalmaClient *protocol);

	virtual void handlePacket(Packet *pkt) {}
	virtual void clean() {}
	bool validSet(Packet *pkt);
	bool validOffer(Packet *pkt);
};

class DiscoveryState : public State
{
public:
	class DiscoveryTimer : public Timer
	{
	public:
		DiscoveryState *m_state;
		DiscoveryTimer(DiscoveryState *state);
		void timeout();
	};
	DiscoveryTimer m_discovery_timer;
	int m_dsc_count; 
	uint64_t m_src_addr;
	Packet *m_offer;
	AddrSet m_discovery_set;
	bool m_change_discovery;

	DiscoveryState(PalmaClient *protocol);
	void start();
	void clean();
	void sendDiscover();
	void handlePacket(Packet *pkt);
	void checkSet(AddrSet *set, uint16_t lifetime);
	uint64_t getSrcAddr();
	void storeOffer(Packet *pkt);
};

class RequestingState : public State
{
public:
	class RequestTimer : public Timer
	{
	public:
		RequestingState *m_state;
		RequestTimer(RequestingState *state);
		void timeout();
	};
	RequestTimer m_request_timer;
	int m_req_count;
	uint64_t m_server_addr;
	uint64_t m_src_addr;
	Packet *m_pkt;

	RequestingState(PalmaClient *protocol);
	void start(uint64_t server_addr, uint64_t src_addr, AddrSet *set, bool renewal = false);
	void clean();
	void sendRequest();
	void handlePacket(Packet *pkt);
	bool checkNetworkId(uint8_t *rcv, uint8_t *snd);
};

class DefendingState : public State
{
public:
	class LeaseLifetimeTimer : public Timer
	{
	public:
		DefendingState *m_state;
		LeaseLifetimeTimer(DefendingState *state);
		void timeout();
	};
	class AnnouncementTimer : public Timer
	{
	public:
		DefendingState *m_state;
		AnnouncementTimer(DefendingState *state);
		void timeout();
	};
	LeaseLifetimeTimer m_lease_lifetime_timer;
	AnnouncementTimer m_announcement_timer;

	DefendingState(PalmaClient *protocol);
	void start(AddrSet *set);
	void clean();
	void sendAnnounce();
	void sendDefend(AddrSet *original_set, AddrSet *conflict_set, uint64_t DA, uint8_t *station_id = NULL);
	void handlePacket(Packet *pkt);
	void processConflict(AddrSet* original_set, AddrSet *conflict_set, uint64_t DA, uint8_t *station_id = NULL);
};

class BoundState : public State
{
public:
	class LeaseLifetimeTimer : public Timer
	{
	public:
		BoundState *m_state;
		LeaseLifetimeTimer(BoundState *state);
		void timeout();
	};
	LeaseLifetimeTimer m_lease_lifetime_timer;

	BoundState(PalmaClient *protocol);
	void start(uint16_t lifetime, bool acceptable);
	void clean();
	void sendRelease(bool terminate = false);
};

#endif
