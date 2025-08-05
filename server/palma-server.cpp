#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "palma-server.h"
#include "../common/details.h"

#define MIN(a,b) ((a < b) ? a : b)

PalmaServer::PalmaServer() : 	m_db_unicast(this),
								m_db_multicast(this),
								m_db_unicast_64(this),
								m_db_multicast_64(this),
								m_src_addr(0) {}

void PalmaServer::begin()
{
	m_netitf.init(TO_STRING(m_config.get(ConfigItem::INTERFACE)));
	m_event_loop.regSource(&m_netitf);
	m_event_loop.regHandler(this);
	m_src_addr = TO_ADDR(m_config.get(ConfigItem::SRC_ADDR));
	m_netitf.addAddr(m_src_addr);

	AddrSet *unicast_set = TO_ADDRSET_PTR(m_config.get(ConfigItem::UNICAST_SET));
	AddrSet *multicast_set = TO_ADDRSET_PTR(m_config.get(ConfigItem::MULTICAST_SET));
	AddrSet *unicast_64_set = TO_ADDRSET_PTR(m_config.get(ConfigItem::UNICAST_64_SET));
	AddrSet *multicast_64_set = TO_ADDRSET_PTR(m_config.get(ConfigItem::MULTICAST_64_SET));
	m_db_unicast.init(unicast_set);
	m_db_multicast.init(multicast_set);
	m_db_unicast_64.init(unicast_64_set);
	m_db_multicast_64.init(multicast_64_set);
	m_event_loop.run();
}

void PalmaServer::handlePacket(Packet *pkt)
{
	if(pkt->getDA() == PALMA_MCAST)
	{
		switch(pkt->getType())
		{
			case MsgType::DISCOVER:
				processClaim(pkt);
				break;
			case MsgType::ANNOUNCE:
				if(TO_BOOL(m_config.get(ConfigItem::AUTOASSIGN_OBJECTION)))
					processClaim(pkt);
				break;
		}
	}
	else if(pkt->getDA() == m_src_addr)
	{
		switch(pkt->getType())
		{
			case MsgType::REQUEST:
				processRequest(pkt);
				break;
			case MsgType::RELEASE:
				processRelease(pkt);
				break;
		}
	}
}

bool PalmaServer::defineSet(bool isMulticast, bool isSize64, SetDatabase *&db, uint64_t *max_addr, uint16_t *lifetime, bool *send_client_addr)
{
	uint64_t max_addr_unicast = TO_SIZE(m_config.get(ConfigItem::MAX_ADDR_UNICAST));
	uint64_t max_addr_multicast = TO_SIZE(m_config.get(ConfigItem::MAX_ADDR_MULTICAST));
	uint64_t max_addr_unicast_64 = TO_SIZE(m_config.get(ConfigItem::MAX_ADDR_UNICAST_64));	
	uint64_t max_addr_multicast_64 = TO_SIZE(m_config.get(ConfigItem::MAX_ADDR_MULTICAST_64));
	if(send_client_addr != NULL)
		*send_client_addr = true;

	if(isMulticast)
		{
			if(!isSize64)
			{
				if(max_addr_multicast <= 0)
					return false;
				db = &m_db_multicast;
				if(max_addr != NULL)
					*max_addr = max_addr_multicast;
				if(lifetime != NULL)
					*lifetime = TO_UINT(m_config.get(ConfigItem::MULTICAST_LIFETIME));
			}
			else
			{
				if(max_addr_multicast_64 <= 0)
					return false;
				db = &m_db_multicast_64;
				if(max_addr != NULL)
					*max_addr = max_addr_multicast_64;
				if(lifetime != NULL)
					*lifetime = TO_UINT(m_config.get(ConfigItem::MULTICAST_64_LIFETIME));
			}
		}		
		else
		{
			if(isSize64)
			{
				if(max_addr_unicast_64 <= 0)
					return false;
				db = &m_db_unicast_64;
				if(max_addr != NULL)
					*max_addr = max_addr_unicast_64;
				if(lifetime != NULL)
					*lifetime = TO_UINT(m_config.get(ConfigItem::UNICAST_64_LIFETIME));
			}
			else
			{
				if(max_addr_unicast <= 0)
					return false;
				db = &m_db_unicast;
				if(max_addr != NULL)
					*max_addr = max_addr_unicast;
				if(lifetime != NULL)
					*lifetime = TO_UINT(m_config.get(ConfigItem::UNICAST_LIFETIME));	
				if(send_client_addr != NULL)
					*send_client_addr = false;
			}

		}
	return true;
}

void PalmaServer::processClaim(Packet *pkt)
{
	uint64_t src_addr = pkt->getSA();
	uint8_t *station_id = pkt->getStationId();
	uint16_t token = pkt->getToken();
	AddrSet *claimed_set = pkt->getSet();
	uint64_t security_id = getSecurityId(token, station_id);
	bool isMulticast;
	bool isSize64;
	uint64_t max_addr_offer;
	AddrSet *offer_set;
	SetDatabase *db;
	uint16_t lifetime;
	bool send_client_addr;
	uint64_t max_addr;
		
	if(claimed_set == NULL)
	{
		isSize64 = TO_BOOL(m_config.get(ConfigItem::DEFAULT_64));
		isMulticast = TO_BOOL(m_config.get(ConfigItem::DEFAULT_MULTICAST));
		max_addr_offer = TO_SIZE(m_config.get(ConfigItem::DEFAULT_ADDR_OFFER));
	}
	else
	{
		isSize64 =  claimed_set->isSize64();
		isMulticast = claimed_set->isMulticast();
		max_addr_offer = claimed_set->getSize();
	}
	
	if(!defineSet(isMulticast, isSize64, db, &max_addr, &lifetime, &send_client_addr))
		return;
	
	AddrSet src_addr_set(src_addr);
	AddrSet check_set;
	AddrSet *client_addr = NULL;

	max_addr_offer = MIN(max_addr, max_addr_offer);
	offer_set = db->reserve(max_addr_offer, security_id, TO_UINT(m_config.get(ConfigItem::RESERVE_LIFETIME)));
	if(offer_set != NULL)
	{
		if(send_client_addr && check_set.checkConflict(&src_addr_set, &DISCOVER_SOURCE_ADDR_RANGE))
		{
			client_addr = m_db_unicast.reserve(1, security_id, TO_UINT(m_config.get(ConfigItem::RESERVE_LIFETIME)));
			if(client_addr == NULL)
			{
				db->release((AssignableSet*)offer_set);
				return;
			}
		}
		sendOffer(src_addr, token, offer_set, lifetime, station_id, client_addr);
	}
}


void PalmaServer::sendOffer(uint64_t dest_addr, uint16_t token, AddrSet *offer_set, 
				uint16_t lifetime, uint8_t *station_id, AddrSet *client_addr)
{
	uint8_t *id;
	
	Packet pkt(MsgType::OFFER, dest_addr, m_src_addr, token);
	pkt.addLifetimePar(lifetime);
	if(offer_set->getSize() > 0xffff)
		offer_set->alignToMask(SetType::MASK);
	pkt.addMacSetPar(offer_set);
	if(station_id != NULL)
		pkt.addIdPar(ParType::STATION_ID, station_id);
	id = TO_STRING(m_config.get(ConfigItem::NETWORK_ID));
	if(id != NULL)
		pkt.addIdPar(ParType::NETWORK_ID, id);
	if(client_addr != NULL)
		pkt.addClientAddrPar(client_addr);
	id = TO_STRING(m_config.get(ConfigItem::VENDOR));
	if(id != NULL)
		pkt.addVendorPar(id, strlen((const char *)id));
	m_netitf.netsend(&pkt);
}

void PalmaServer::processRequest(Packet *pkt)
{
	uint64_t security_id;
	AddrSet *requested_set = pkt->getSet();
	uint8_t * station_id = pkt->getStationId();
	uint16_t token = pkt->getToken();
	uint64_t src_addr = pkt->getSA();
	uint64_t reserved_security_id = getSecurityId(token, station_id);
	uint64_t assigned_security_id = getSecurityId(token, station_id, src_addr);

	bool isSize64 =  requested_set->isSize64();
	bool isMulticast = requested_set->isMulticast();
	uint64_t max_addr;
	uint16_t lifetime;
	SetDatabase *db;
	
	AddrSet src_addr_set(src_addr);
	AddrSet check_set;

	if(!defineSet(isMulticast, isSize64, db, &max_addr, &lifetime)
		|| check_set.checkConflict(&src_addr_set, &AUTOASSIGN_UNICAST)
		|| check_set.checkConflict(&src_addr_set, &DISCOVER_SOURCE_ADDR_RANGE) )
	{
		sendAck(src_addr, token, station_id, StatusCode::FAIL_OTHER);
		return;
	}
	
	DbStatus db_status;
	uint16_t left_lifetime;
	bool identical;
	AssignableSet *src_assign_set = NULL;
	AssignableSet *result;

	if(check_set.checkConflict(&src_addr_set, TO_ADDRSET_PTR(m_config.get(ConfigItem::UNICAST_SET))))
	{
		db_status = m_db_unicast.checkStatus(&src_addr_set, security_id, left_lifetime, identical, result);
		if((db_status == DbStatus::RESERVED && security_id == reserved_security_id)
			|| (db_status == DbStatus::ASSIGNED && security_id == assigned_security_id))
		{
			if(!check_set.checkConflict(&src_addr_set, requested_set))
				src_assign_set = result;
		}
		else
		{
			sendAck(src_addr, token, station_id, StatusCode::FAIL_OTHER);
			return;
		}
	}
	StatusCode ack_status = StatusCode::ASSIGN_OK;
	if(requested_set->getSize() > max_addr)
	{
		if(TO_BOOL(m_config.get(ConfigItem::ENABLE_ALTERNATE_SET)))
		{
			requested_set->setSize(max_addr);
			ack_status = StatusCode::ALTERNATE_SET;
		}
		else
		{
			sendAck(src_addr, token, station_id, StatusCode::FAIL_TOO_LARGE);
			return;
		}
				
	}
	
	db_status = db->checkStatus(requested_set, security_id, left_lifetime, identical, result);
	
	if (db_status == DbStatus::FREE 
			|| ((db_status == DbStatus::RESERVED) && (security_id == reserved_security_id))
			|| (((db_status == DbStatus::ASSIGNED) && (security_id == assigned_security_id))
				&& (pkt->getRenewal() && TO_BOOL(m_config.get(ConfigItem::ACCEPT_RENEWAL)))))
	{
		if(src_assign_set != NULL)
			m_db_unicast.assign(src_assign_set, &src_addr_set, assigned_security_id, lifetime);
		db->assign(result, requested_set, assigned_security_id, lifetime);
		sendAck(src_addr, token, station_id, ack_status, requested_set, lifetime);
	}

	else if(db_status == DbStatus::ASSIGNED && security_id == assigned_security_id)
		sendAck(src_addr, token, station_id, ack_status, requested_set, left_lifetime);

	else if(TO_BOOL(m_config.get(ConfigItem::ENABLE_ALTERNATE_SET)))
	{
		AddrSet *set = db->assign(requested_set->getSize(), assigned_security_id, lifetime);
		if(set == NULL)
			sendAck(src_addr, token, station_id, StatusCode::FAIL_CONFLICT);
		else
		{
			if(src_assign_set != NULL)
				m_db_unicast.assign(src_assign_set, &src_addr_set, assigned_security_id, lifetime);
			sendAck(src_addr, token, station_id, StatusCode::ALTERNATE_SET, set, lifetime);
		}
	}
	else
		sendAck(src_addr, token, station_id, StatusCode::FAIL_CONFLICT);									
}

void PalmaServer::sendAck(uint64_t dest_addr, uint16_t token, uint8_t *station_id, StatusCode status, AddrSet *set, uint16_t lifetime)
{
	Packet pkt(MsgType::ACK, dest_addr, m_src_addr, token, status);
	if(station_id != NULL)
		pkt.addIdPar(ParType::STATION_ID, station_id);
	if(set != NULL)
	{
		pkt.addMacSetPar(set);
		pkt.addLifetimePar(lifetime);
	}
	m_netitf.netsend(&pkt);
}

void PalmaServer::processRelease(Packet *pkt)
{
	
	AddrSet *released_set = pkt->getSet();
	uint8_t * station_id = pkt->getStationId();
	uint16_t token = pkt->getToken();
	uint64_t src_addr = pkt->getSA();
	uint64_t security_id;
	uint16_t left_lifetime;
	uint64_t assigned_security_id = getSecurityId(token, station_id, src_addr);
	AddrSet src_addr_set(src_addr);
	SetDatabase *db;
	bool isMulticast = released_set->isMulticast();
	bool isSize64 = released_set->isSize64();

	if(!defineSet(isMulticast, isSize64, db))
		return;
	
	bool identical;
	AssignableSet *result;
	if(db->checkStatus(released_set, security_id, left_lifetime, identical, result) == DbStatus::ASSIGNED
		&& security_id == assigned_security_id && identical)
	{
		db->release(result);
		if(m_db_unicast.checkStatus(&src_addr_set, security_id, left_lifetime, identical, result) == DbStatus::ASSIGNED
			&& security_id == assigned_security_id && identical)
			m_db_unicast.release(result);
	}
}

uint64_t PalmaServer::getSecurityId(uint16_t token, uint8_t *station_id, uint64_t src_addr)
{
	m_hash.begin();
	if(src_addr != 0)
		m_hash.update(src_addr);
	m_hash.update(token);
	m_hash.update(station_id);
	return m_hash.digest();
}

void PalmaServer::onExit()
{
	printf("ENDING\n");
}
