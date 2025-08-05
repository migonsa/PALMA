#ifndef PALMA_CLIENT_H
#define PALMA_CLIENT_H

#include "states.h"
#include "config-client.h"
#include "../common/database.h"
#include "../common/palma.h"

class PalmaClient : public Palma
{
public:
	ConfigClient m_config;
	SetDatabase m_db;
	State *m_curstate;
	uint16_t m_token;
	bool m_mcast_on;
	bool m_preassigned_addr;
	uint64_t m_src_addr;
	uint64_t m_server_addr;
	AddrSet m_assigned_set;
	
	DiscoveryState m_discovery_state;
	RequestingState m_requesting_state;
	BoundState m_bound_state;
	DefendingState m_defending_state;

	PalmaClient();
	void begin();
	void handlePacket(Packet *pkt);
	void restart();
	bool checkStationId(uint8_t *station_id);
	void updateToken();	
	uint16_t getToken();
	void onExit();
	void printv(const char *format, ...);
};

#endif
