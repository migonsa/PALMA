#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "palma-client.h"

PalmaClient::PalmaClient() :
								m_db(this),
								m_curstate(NULL), 
								m_token((uint16_t)lrand48()),
								m_mcast_on(false),
								m_src_addr(0),
								m_server_addr(0),
								m_preassigned_addr(false),
								m_discovery_state(this),
								m_requesting_state(this),
								m_bound_state(this),
								m_defending_state(this) {}

void PalmaClient::begin()
{
	printv("BEGIN\n");
	m_netitf.init(TO_STRING(m_config.get(ConfigItem::INTERFACE)));
	m_event_loop.regSource(&m_netitf);
	m_event_loop.regHandler(this);
	m_src_addr = TO_ADDR(m_config.get(ConfigItem::PREASSIGNED_ADDR));
	if(m_src_addr)
	{
		m_netitf.addAddr(m_src_addr);
		m_preassigned_addr = true;
	}
	m_server_addr = TO_ADDR(m_config.get(ConfigItem::KNOWN_SERVER_ADDR));
	AddrSet *claim_set = TO_ADDRSET_PTR(m_config.get(ConfigItem::CLAIM_SET));
	m_db.init(claim_set);
	if(m_server_addr && m_preassigned_addr)
		m_requesting_state.start(m_server_addr, m_src_addr, claim_set);
	else
		m_discovery_state.start();
	m_event_loop.run();
}

void PalmaClient::handlePacket(Packet *pkt)
{
	m_curstate->handlePacket(pkt);
}

void PalmaClient::restart()
{
	printv("RESTARTING\n");
	m_curstate->clean();
	if(!m_preassigned_addr)
	{
		if(m_src_addr != 0)
		{
			m_netitf.delAddr(m_src_addr);
			m_src_addr = 0;
		}
	}
	m_server_addr = 0;
	m_assigned_set = AddrSet();
	updateToken();
	m_server_addr = TO_ADDR(m_config.get(ConfigItem::KNOWN_SERVER_ADDR));
	AddrSet *claim_set = TO_ADDRSET_PTR(m_config.get(ConfigItem::CLAIM_SET));
	if(m_server_addr && m_preassigned_addr)
		m_requesting_state.start(m_server_addr, m_src_addr, claim_set);
	else
		m_discovery_state.start();
}

bool PalmaClient::checkStationId(uint8_t *station_id)
{
	uint8_t *id = TO_STRING(m_config.get(ConfigItem::STATION_ID));
	if(station_id == NULL ||
			(id != NULL && 
			strcmp((const char *)station_id, (const char *)id) == 0))
		return true;
	return false; 
}

void PalmaClient::updateToken()
{
	 m_token++;
}

uint16_t PalmaClient::getToken()
{
	return m_token;
}

void PalmaClient::onExit()
{
	if(m_curstate == &m_bound_state)
		m_bound_state.sendRelease(true);
	m_curstate->clean();
	printv("ENDING\n");
}

void PalmaClient::printv(const char *format, ...)
{
	if(TO_BOOL(m_config.get(ConfigItem::VERBOSE)))
	{
		Time now;
		va_list args;
	  	va_start(args, format);
		printf("%.6f,", now.get());
	 	vprintf(format, args);
	 	va_end(args);
		fflush(stdout);
	}
}
