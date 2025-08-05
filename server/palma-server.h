#ifndef PALMA_SERVER_H
#define PALMA_SERVER_H

#include "../common/netitf.h" 
#include "../common/eventloop.h"
#include "config-server.h"
#include "../common/database.h"
#include "../common/siphash.h"
#include "../common/palma.h"

class PalmaServer : public Palma
{
public:
	ConfigServer m_config;
	SetDatabase m_db_unicast;
	SetDatabase m_db_multicast;
	SetDatabase m_db_unicast_64;
	SetDatabase m_db_multicast_64;
	SipHash m_hash;
	uint64_t m_src_addr;

	PalmaServer();
	void begin();
	void handlePacket(Packet *pkt);
	bool defineSet(bool isMulticast, bool isSize64, SetDatabase *&db, 
					uint64_t *max_addr = NULL, uint16_t *lifetime = NULL, bool *send_client_addr = NULL);
	void processClaim(Packet *pkt);
	void sendOffer(uint64_t dest_addr, uint16_t token, AddrSet *offer_set, 
						uint16_t lifetime, uint8_t *station_id = NULL, AddrSet *client_addr = NULL);
	void processRequest(Packet *pkt);
	void sendAck(uint64_t dest_addr, uint16_t token, uint8_t *station_id, 
						StatusCode status, AddrSet *set = NULL, uint16_t lifetime = 0);
	void processRelease(Packet *pkt);
	uint64_t getSecurityId(uint16_t token, uint8_t *station_id, uint64_t src_addr = 0);
	void onExit();
};

#endif
