#include "states.h"
#include "../common/details.h"
#include "palma-client.h"

#define MIN(a,b) ((a < b) ? a : b)

State::State(PalmaClient *protocol) : m_protocol(protocol) {}

static void adjustSet(AddrSet *set, uint64_t max)
{
	uint64_t size = MIN(set->getSize(), max);
	set->setSize(size);
	if(size > 0xffff)
		set->alignToMask(SetType::MASK);
}

DiscoveryState::DiscoveryState(PalmaClient *protocol) : State(protocol), 
														m_discovery_timer(this) {}

DiscoveryState::DiscoveryTimer::DiscoveryTimer(DiscoveryState *state) : m_state(state) {}

void DiscoveryState::start()
{
	m_protocol->printv("STARTING\n");
	m_protocol->m_curstate = this;
	m_dsc_count = DISCOVER_DSC_COUNT;
	m_src_addr = 0;
	uint64_t min = TO_SIZE(m_protocol->m_config.get(ConfigItem::MIN_ADDR_CLAIM));
	uint64_t max = TO_SIZE(m_protocol->m_config.get(ConfigItem::MAX_ADDR_CLAIM));
	bool random = TO_BOOL(m_protocol->m_config.get(ConfigItem::RANDOM_ASSIGN));
	m_offer = NULL;
	AddrSet *set = m_protocol->m_db.getFreeSet(min, max, random);
	if(set != NULL)
		m_discovery_set = *set;
	else
		m_discovery_set.setSize(0);
	if(m_discovery_set.getSize() > 0xffff)
		m_discovery_set.alignToMask(SetType::MASK);
	m_change_discovery = false;
	if(!m_protocol->m_mcast_on)
	{
		m_protocol->m_netitf.addAddr(PALMA_MCAST,true);
		m_protocol->m_mcast_on = true;
	}
	sendDiscover();
}

void DiscoveryState::clean()
{
	if(m_src_addr && !m_protocol->m_preassigned_addr)
	{
		m_protocol->m_netitf.delAddr(m_src_addr);	
		m_src_addr = 0;
	}
	delete m_offer;
	m_offer = NULL;
	m_discovery_set = AddrSet();
	m_change_discovery = false;
	m_protocol->m_event_loop.stopTimer(&m_discovery_timer);
}

void DiscoveryState::sendDiscover()
{
	Packet pkt(MsgType::DISCOVER, PALMA_MCAST, getSrcAddr(), m_protocol->getToken());
	if(m_discovery_set.getSize() > 0)
		pkt.addMacSetPar(&m_discovery_set);
	if(TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)) != NULL)
		pkt.addIdPar(ParType::STATION_ID, TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)));
	uint8_t *vendor_specific = TO_STRING(m_protocol->m_config.get(ConfigItem::VENDOR));
	if(vendor_specific != NULL)
		pkt.addVendorPar(vendor_specific, strlen((const char *)vendor_specific));
	m_protocol->m_netitf.netsend(&pkt);
	m_protocol->m_event_loop.startTimer(&m_discovery_timer, DISCOVERY_TIMEOUT);
}

void DiscoveryState::handlePacket(Packet *pkt)
{
	if((pkt->getDA() == PALMA_MCAST && pkt->getType() != MsgType::ANNOUNCE))
		return;
	else if((pkt->getDA() != m_src_addr 
				|| !m_protocol->checkStationId(pkt->getStationId()))
				&& pkt->getType() != MsgType::ANNOUNCE)
		return;
	switch(pkt->getType())
	{
		case MsgType::OFFER:
			if(pkt->getToken() == m_protocol->getToken())
				storeOffer(pkt);
			break;
		case MsgType::ANNOUNCE:
			checkSet(pkt->getSet(), pkt->getLifetime());
			break;
		case MsgType::DEFEND:
			checkSet(pkt->getSet(false), pkt->getLifetime());
			break;		
	}
}

void DiscoveryState::checkSet(AddrSet *set, uint16_t lifetime) //return true si colisiona con tu discover set y apunta las colisiones en DB
{
	AddrSet conflict_set;
	m_protocol->m_db.exclude(set, lifetime);
	if(conflict_set.checkConflict(set, &m_discovery_set))
		m_change_discovery = true;
}

uint64_t DiscoveryState::getSrcAddr()
{
	if(m_protocol->m_preassigned_addr)
	{
		m_src_addr = m_protocol->m_src_addr;
	}
	else
	{ 
		RandomAddrSet src_addr(DISCOVER_SOURCE_ADDR_RANGE);
		if (m_src_addr != 0)
			m_protocol->m_netitf.delAddr(m_src_addr);
		m_src_addr = src_addr.getFirstAddr();
		m_protocol->m_netitf.addAddr(m_src_addr);
	}
	return m_src_addr;
}

bool State::validSet(Packet *pkt)
{
	return (pkt->getSet()->getSize() >= TO_SIZE(m_protocol->m_config.get(ConfigItem::MIN_ADDR_CLAIM))
			&& (TO_ADDRSET_PTR(m_protocol->m_config.get(ConfigItem::CLAIM_SET))->isMulticast() == pkt->getSet()->isMulticast())
			&& (TO_ADDRSET_PTR(m_protocol->m_config.get(ConfigItem::CLAIM_SET))->isSize48() == pkt->getSet()->isSize48()));
}

bool State::validOffer(Packet *pkt)
{
	return (validSet(pkt) && (!TO_ADDRSET_PTR(m_protocol->m_config.get(ConfigItem::CLAIM_SET))->isMulticast() 
								|| m_protocol->m_preassigned_addr || pkt->getClientAddr()));
}

void DiscoveryState::storeOffer(Packet *pkt)
{
	uint pkt_set_size = MIN(pkt->getSet()->getSize(), TO_SIZE(m_protocol->m_config.get(ConfigItem::MAX_ADDR_CLAIM)));
	//mirar si el pkt es aceptable o no
	if(validOffer(pkt) && (m_offer == NULL 
								||(m_offer->getSet()->getSize() < pkt_set_size)
								||(m_offer->getSet()->getSize() == pkt_set_size
									&& m_offer->getLifetime() < pkt->getLifetime())))
	{
	//tamaño del set, si es multicast o no el de config y el del offer, si lleva clientAddr y lo necesito y si el lftimer/set es mayor que el del offer que ya tengo en m_offer
		delete m_offer;
		m_offer = new Packet(pkt);
	}
	return;
}

void DiscoveryState::DiscoveryTimer::timeout()
{
	AddrSet aux_set;
	if(m_state->m_offer != NULL)
	{
		uint64_t server_addr = m_state->m_offer->getSA();
		uint64_t src_addr = m_state->m_src_addr;
		AddrSet offered_set(*m_state->m_offer->getSet());
		if(!m_state->m_protocol->m_preassigned_addr)
		{
			src_addr = m_state->m_offer->getClientAddr();
			if(!src_addr && !offered_set.isMulticast())
				 src_addr = offered_set.getFirstAddr();
		}
		m_state->clean();
		m_state->m_protocol->m_requesting_state.start(server_addr, src_addr, &offered_set);
	}
	else if(m_state->m_change_discovery == true)
		m_state->m_protocol->restart();
	else if(--(m_state->m_dsc_count) > 0)
		m_state->sendDiscover();
	else if(aux_set.checkConflict(&m_state->m_discovery_set, &AUTOASSIGN_UNICAST)
				|| ((aux_set.checkConflict(&m_state->m_discovery_set, &AUTOASSIGN_MULTICAST)
						|| aux_set.checkConflict(&m_state->m_discovery_set, &AUTOASSIGN_UNICAST_64)
						|| aux_set.checkConflict(&m_state->m_discovery_set, &AUTOASSIGN_MULTICAST_64))
					&& m_state->m_protocol->m_preassigned_addr))
	{
		AddrSet assigned_set = m_state->m_discovery_set;
		uint64_t size = TO_SIZE(m_state->m_protocol->m_config.get(ConfigItem::MAX_ADDR_CLAIM));
		if(!assigned_set.isMulticast())
			size = MIN(size, MAX_AUTOASSIGN_UNICAST);
		if(TO_BOOL(m_state->m_protocol->m_config.get(ConfigItem::RANDOM_ASSIGN)))
			assigned_set = RandomAddrSet(assigned_set, size);
		else
			adjustSet(&assigned_set, size);
		m_state->clean();
		m_state->m_protocol->m_defending_state.start(&assigned_set);
	}
	else
		m_state->m_protocol->restart(); 
}


RequestingState::RequestingState(PalmaClient *protocol) : State(protocol),
															m_pkt(NULL),
															m_request_timer(this) {}

RequestingState::RequestTimer::RequestTimer(RequestingState *state) : m_state(state) {}

void RequestingState::start(uint64_t server_addr, uint64_t src_addr, AddrSet *set, bool renewal)
{
	m_protocol->printv("SERVER_REQUESTING,0x%lx,0x%lx\n", 
			set->getFirstAddr(), 
			set->getSize());
	m_protocol->m_curstate = this;
	if(m_protocol->m_mcast_on)
	{
		m_protocol->m_netitf.delAddr(PALMA_MCAST,true);
		m_protocol->m_mcast_on = false;
	}
	m_req_count = REQUEST_REQ_COUNT;
	m_server_addr = server_addr;
	m_src_addr = src_addr;
	m_pkt = new Packet(MsgType::REQUEST, server_addr, src_addr, m_protocol->getToken());
	adjustSet(set, TO_SIZE(m_protocol->m_config.get(ConfigItem::MAX_ADDR_CLAIM)));
	m_pkt->addMacSetPar(set);
	if(TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)) != NULL)
		m_pkt->addIdPar(ParType::STATION_ID, TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)));
	if(renewal)
		m_pkt->setRenewal();
	else if(!m_protocol->m_preassigned_addr)
		m_protocol->m_netitf.addAddr(src_addr);
	sendRequest();
}

void RequestingState::clean()
{
	m_server_addr = 0;
	if(m_src_addr != 0)
	{
		m_protocol->m_netitf.delAddr(m_src_addr);
		m_src_addr = 0;
	}
	if(m_pkt)
	{
		delete m_pkt;
		m_pkt = NULL;
	}
	m_protocol->m_event_loop.stopTimer(&m_request_timer);
}

void RequestingState::sendRequest()
{
	m_protocol->m_netitf.netsend(m_pkt);
	m_protocol->m_event_loop.startTimer(&m_request_timer, REQUESTING_TIMEOUT);
}

void RequestingState::handlePacket(Packet *pkt)
{
	if(pkt->getType() == MsgType::ACK &&
			pkt->getDA() == m_src_addr &&
			pkt->getSA() == m_server_addr &&
			pkt->getToken() == m_protocol->getToken() && 
			m_protocol->checkStationId(pkt->getStationId())) 
	{
		uint16_t lifetime = pkt->getLifetime();
		if((pkt->getStatus() == StatusCode::ASSIGN_OK 
				|| pkt->getStatus() == StatusCode::ALTERNATE_SET)
					&& lifetime > 0)
		{
			m_protocol->m_src_addr = m_src_addr;
			m_src_addr = 0;
			m_protocol->m_server_addr = m_server_addr;
			m_protocol->m_assigned_set = *pkt->getSet();
			clean();
			m_protocol->m_bound_state.start(lifetime, validSet(pkt));
		}
		else
		{
			m_protocol->restart();
		}
	}
}

bool RequestingState::checkNetworkId(uint8_t *rcv, uint8_t *snd)
{
	if(rcv == NULL ||
			(snd != NULL && strcmp((const char *)rcv, (const char *)snd) == 0))
		return true;
	return false; 
}

void RequestingState::RequestTimer::timeout()
{
	if(--(m_state->m_req_count) > 0)
	{
		m_state->sendRequest();
	}
	else
	{
		m_state->m_protocol->restart();
	}
}


DefendingState::DefendingState(PalmaClient *protocol) : State(protocol),
													m_lease_lifetime_timer(this),
													m_announcement_timer(this) {}

DefendingState::LeaseLifetimeTimer::LeaseLifetimeTimer(DefendingState *state) : m_state(state) {}

DefendingState::AnnouncementTimer::AnnouncementTimer(DefendingState *state) : m_state(state) {}


void DefendingState::start(AddrSet *set)
{
	m_protocol->m_curstate = this;
	if(!m_protocol->m_preassigned_addr)
	{
		RandomAddrSet src_addr(*set);
		m_protocol->m_src_addr = src_addr.getFirstAddr();
		m_protocol->m_netitf.addAddr(m_protocol->m_src_addr);
	}
	m_protocol->m_assigned_set = *set;
	m_protocol->printv("AUTO_ASSIGNED,0x%lx,0x%lx\n", 
			m_protocol->m_assigned_set.getFirstAddr(), 
			m_protocol->m_assigned_set.getSize());
	m_protocol->m_event_loop.startTimer(&m_lease_lifetime_timer,SELF_ASSIGMENT_LIFETIME);	
	sendAnnounce();
}

void DefendingState::clean()
{
	if(!m_protocol->m_preassigned_addr)
	{
		m_protocol->m_netitf.delAddr(m_protocol->m_src_addr);
		m_protocol->m_src_addr = 0;
	}
	m_protocol->m_assigned_set = AddrSet();
	m_protocol->m_event_loop.stopTimer(&m_announcement_timer);
	m_protocol->m_event_loop.stopTimer(&m_lease_lifetime_timer);
}

void DefendingState::sendAnnounce()
{
	Packet pkt(MsgType::ANNOUNCE, PALMA_MCAST, m_protocol->m_src_addr, m_protocol->getToken());
	pkt.addMacSetPar(&m_protocol->m_assigned_set);
	pkt.addLifetimePar((uint16_t)(m_protocol->m_event_loop.readTimer(&m_lease_lifetime_timer) + 0.5));
	if(TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)) != NULL)
		pkt.addIdPar(ParType::STATION_ID, TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)));
	m_protocol->m_netitf.netsend(&pkt);
	m_protocol->m_event_loop.startTimer(&m_announcement_timer, ANNOUNCE_TIMEOUT);
}

void DefendingState::sendDefend(AddrSet* original_set, AddrSet *conflict_set, uint64_t DA, uint8_t *station_id)
{
	Packet pkt(MsgType::DEFEND, DA, m_protocol->m_src_addr, m_protocol->getToken());
	if(station_id != NULL)
		pkt.addIdPar(ParType::STATION_ID, station_id);
	pkt.addLifetimePar((uint16_t)(m_protocol->m_event_loop.readTimer(&m_lease_lifetime_timer) + 0.5));
	pkt.addMacSetPar(original_set);
	pkt.addMacSetPar(conflict_set, false);
	m_protocol->m_netitf.netsend(&pkt);
}

void DefendingState::handlePacket(Packet *pkt)
{
	AddrSet conflict_set;	
	if(pkt->getDA() == PALMA_MCAST)
	{
		if(pkt->getType() == MsgType::ANNOUNCE)
		{
			m_protocol->m_db.exclude(pkt->getSet(), pkt->getLifetime());
			if(conflict_set.checkConflict(pkt->getSet(),&m_protocol->m_assigned_set))
				processConflict(pkt->getSet(), &conflict_set, pkt->getSA(), pkt->getStationId());
		}
		else if(pkt->getType() == MsgType::DISCOVER && 
				conflict_set.checkConflict(pkt->getSet(), &m_protocol->m_assigned_set))
		{	
			sendDefend(pkt->getSet(), &conflict_set, pkt->getSA(), pkt->getStationId());
		}
	}
	else if(pkt->getDA() == m_protocol->m_src_addr &&
			m_protocol->checkStationId(pkt->getStationId()))
	{
		if(pkt->getType() == MsgType::DEFEND)
		{
			m_protocol->m_db.exclude(pkt->getSet(false), pkt->getLifetime());
			if(conflict_set.checkConflict(pkt->getSet(false),&m_protocol->m_assigned_set))
			{
				processConflict(pkt->getSet(false), &conflict_set, pkt->getSA());
			}
			
		}
		else if(pkt->getType() == MsgType::OFFER && pkt->getToken() == m_protocol->getToken())
		{
			if(validOffer(pkt))
			{
				AddrSet offered_set(*pkt->getSet());
				uint64_t server_addr = pkt->getSA();
				uint64_t src_addr = m_protocol->m_src_addr;
				if(!m_protocol->m_preassigned_addr)
				{
					m_protocol->m_src_addr = 0;
					src_addr = pkt->getClientAddr();
					if(!src_addr && !offered_set.isMulticast())
						src_addr = offered_set.getFirstAddr();						
				}
				clean();
				m_protocol->m_requesting_state.start(server_addr, src_addr, &offered_set);
			}
		}
	}
}

void DefendingState::processConflict(AddrSet *original_set, AddrSet *conflict_set, uint64_t DA, uint8_t *station_id)
{
	
	//solidario
	uint64_t min_assigned = TO_SIZE(m_protocol->m_config.get(ConfigItem::MIN_ADDR_CLAIM));
	uint64_t max_assigned = TO_SIZE(m_protocol->m_config.get(ConfigItem::MAX_ADDR_CLAIM));
	uint64_t left_count = m_protocol->m_assigned_set.getFirstAddr() - conflict_set->getFirstAddr();
	if(left_count >= min_assigned)
	{
		adjustSet(&m_protocol->m_assigned_set, left_count);
	}
	//machito, metemos DB original-conflict+las que liberamos
	else if(min_assigned < m_protocol->m_assigned_set.getSize())
	{
		adjustSet(&m_protocol->m_assigned_set, min_assigned);
		conflict_set->checkConflict(original_set, &m_protocol->m_assigned_set);
		sendDefend(original_set, conflict_set, DA, station_id);
	}
	else
	{ 
		m_protocol->restart();
	}
}

void DefendingState::AnnouncementTimer::timeout()
{
	m_state->sendAnnounce();
}

void DefendingState::LeaseLifetimeTimer::timeout()
{
	m_state->m_protocol->restart();
}

BoundState::BoundState(PalmaClient *protocol) : State(protocol),
												m_lease_lifetime_timer(this) {}

BoundState::LeaseLifetimeTimer::LeaseLifetimeTimer(BoundState *state) : m_state(state) {}

void BoundState::start(uint16_t lifetime, bool acceptable)
{
	m_protocol->printv("SERVER_ASSIGNED,0x%lx,0x%lx\n", 
			m_protocol->m_assigned_set.getFirstAddr(), 
			m_protocol->m_assigned_set.getSize());
	m_protocol->m_curstate = this;
	if(acceptable)
	{
		if(TO_BOOL(m_protocol->m_config.get(ConfigItem::RENEWAL)) && lifetime > 1)
			lifetime -= 1;
		m_protocol->m_event_loop.startTimer(&m_lease_lifetime_timer, (double) lifetime);
	}
	else
		sendRelease();
}

void BoundState::clean()
{
	m_protocol->m_event_loop.stopTimer(&m_lease_lifetime_timer);
}

void BoundState::sendRelease(bool terminate)
{
	Packet pkt(MsgType::RELEASE, m_protocol->m_server_addr, m_protocol->m_src_addr, m_protocol->getToken());
	pkt.addMacSetPar(&m_protocol->m_assigned_set);
	if(TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)) != NULL)
		pkt.addIdPar(ParType::STATION_ID, TO_STRING(m_protocol->m_config.get(ConfigItem::STATION_ID)));
	m_protocol->m_netitf.netsend(&pkt);
	m_protocol->printv("SERVER_RELEASED,0x%lx,0x%lx\n", 
			m_protocol->m_assigned_set.getFirstAddr(), 
			m_protocol->m_assigned_set.getSize());
	if(!terminate)
		m_protocol->restart();
}

void BoundState::LeaseLifetimeTimer::timeout()
{
	if(TO_BOOL(m_state->m_protocol->m_config.get(ConfigItem::RENEWAL)))
	{
		m_state->clean();
		m_state->m_protocol->printv("SERVER_RENEWAL,0x%lx,0x%lx\n", 
			m_state->m_protocol->m_assigned_set.getFirstAddr(), 
			m_state->m_protocol->m_assigned_set.getSize());
		m_state->m_protocol->m_requesting_state.start(m_state->m_protocol->m_server_addr, m_state->m_protocol->m_src_addr, &m_state->m_protocol->m_assigned_set, true);
	}
	else
		m_state->m_protocol->restart();
}

