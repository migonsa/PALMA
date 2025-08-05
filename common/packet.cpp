#include <string.h>
#include <endian.h>
#include "packet.h"
#include "details.h"

#include <stdio.h>

/* Same bit structure as slap_type */

#define SLAP_CW			0x37
#define AAI_CW			(1<<0)
#define ELI_CW			(1<<1)
#define SAI_CW			(1<<2)
#define MAC64_CW		(1<<4)
#define MCAST_CW		(1<<5)

#define SERVER_CW		(1<<6)
#define SETPROV_CW		(1<<7)
#define STATIONID_CW	(1<<8)
#define NETWORKID_CW	(1<<9)
#define CODEFIELD_CW	(1<<10)
#define VENDOR_CW		(1<<11)
#define RENEWAL_CW		(1<<12)


PacketPar::PacketPar(ParType par_id, uint8_t len) : m_par_id(par_id), m_length(len) {}

int PacketPar::toBuffer(uint8_t *&data)
{
	*data++ = (uint8_t) m_par_id;
	*data++ = m_length + 2;
	return 2;
}

IdPar::IdPar(ParType par_id, uint8_t *data, uint8_t len) : PacketPar(par_id, len) //Constructor para sacar el Parameter Station/Network 																						del paquete
{
	m_id = new uint8_t[len+1];
	memcpy(m_id,data,len);
	m_id[len] = 0;
}

IdPar::~IdPar()
{
	delete m_id;
}

int IdPar::toBuffer(uint8_t *&data)
{
	int len = PacketPar::toBuffer(data);
	memcpy(data, m_id, m_length);
	data += m_length;
	return len + m_length;
}

PacketPar* IdPar::clone()
{
	return new IdPar(m_par_id, m_id, m_length);
}

MacSetPar::MacSetPar(uint8_t *data, uint8_t len) : PacketPar(ParType::MAC_ADDR_SET, len) 
{
	uint64_t addr;
	
	if (len < 12)
	{	
		uint16_t count;
		len -= 2;
		addr = Packet::get64(data, len);
		count = Packet::get16(data);
		m_set = AddrSet(addr, count, (len == 6) ? SetSize::SIZE48 : SetSize::SIZE64, SetType::ADDR);
	}
	else
	{
		uint64_t mask;
		len /= 2;
		addr = Packet::get64(data, len);
		mask = Packet::get64(data, len);
		m_set = AddrSet(addr, mask, (len == 6) ? SetSize::SIZE48 : SetSize::SIZE64, SetType::MASK);
	}	
}	

MacSetPar::MacSetPar(AddrSet &set) : m_set(set), PacketPar(ParType::MAC_ADDR_SET, length(set)) {}
 
int MacSetPar::toBuffer(uint8_t *&data)
{
	int len = PacketPar::toBuffer(data);
	Packet::set64(m_set.getFirstAddr(), data, m_set.addrLen());
	if (m_set.getType() == SetType::ADDR)
		Packet::set16(m_set.getSize(), data);
	else
		Packet::set64(m_set.getMask(), data, m_set.addrLen());
	return len + m_length;
}

uint8_t MacSetPar::length(AddrSet &set)
{
	uint8_t len = set.addrLen();
	if (set.m_type == SetType::ADDR)
		return len + 2;
	else
		return len * 2;
}

PacketPar* MacSetPar::clone()
{
	return new MacSetPar(m_set);
}

LifetimePar::LifetimePar(uint8_t *data, uint8_t len) : PacketPar(ParType::LIFETIME, len)
{
	m_lifetime = Packet::get16(data);
}	

LifetimePar::LifetimePar(uint16_t lifetime) : 
				PacketPar(ParType::LIFETIME, 2), m_lifetime(lifetime) {}

int LifetimePar::toBuffer(uint8_t *&data)
{
	int len = PacketPar::toBuffer(data);
	Packet::set16(m_lifetime, data);
	return len + m_length;
}

PacketPar* LifetimePar::clone()
{
	return new LifetimePar(m_lifetime);
}

ClientAddrPar::ClientAddrPar(uint8_t *data, uint8_t len) : PacketPar(ParType::CLIENT_ADDR, len) 
{
	uint64_t addr = Packet::get64(data, len);
	m_set = AddrSet(addr, 1, (len == 6) ? SetSize::SIZE48 : SetSize::SIZE64, SetType::ADDR);	
}	

ClientAddrPar::ClientAddrPar(AddrSet &set) : m_set(set), PacketPar(ParType::CLIENT_ADDR, set.addrLen()) {}
 
int ClientAddrPar::toBuffer(uint8_t *&data)
{
	int len = PacketPar::toBuffer(data);
	Packet::set64(m_set.m_addr, data, m_set.addrLen());
	return len + m_length;
}

PacketPar* ClientAddrPar::clone()
{
	return new ClientAddrPar(m_set);
}

VendorPar::VendorPar(uint8_t *data, uint8_t var_len) : PacketPar(ParType::VENDOR, var_len) 
{
	m_var = new uint8_t[var_len];
	memcpy(m_var,data,var_len);
}

VendorPar::~VendorPar()
{
	delete m_var;
}

int VendorPar::toBuffer(uint8_t *&data)
{
	int len = PacketPar::toBuffer(data);
	memcpy(data, m_var, m_length);
	data += m_length;
	return len + m_length;
}

PacketPar* VendorPar::clone()
{
	return new VendorPar(m_var, m_length);
}

Packet::Packet()
{	
	for(m_num_par = 0; m_num_par < MAX_PAR; m_num_par++)
		m_par[m_num_par] = NULL;
}

Packet::Packet(Packet *pkt)
{
	*this = *pkt;
	for(int num_par = 0; num_par < m_num_par; num_par++)
		m_par[num_par] = pkt->m_par[num_par]->clone();
}

Packet::Packet(MsgType type, uint64_t DA, uint64_t SA, uint16_t token, StatusCode status)
{
	m_DA = DA;
	m_SA = SA;
	m_version = 0;
	m_message_type = type;
	m_control_word = (type == MsgType::OFFER ||
						type == MsgType::ACK) ? SERVER_CW : 0;
	m_control_word |= ((uint8_t)status != 0) ? CODEFIELD_CW : 0;
	m_token = token;
	m_status = status;
	m_length = HEADER_SIZE;
	for(m_num_par = 0; m_num_par < MAX_PAR; m_num_par++)
		m_par[m_num_par] = NULL;
	m_num_par = 0;
}

Packet::~Packet()
{
	for(int m_num_par = 0; m_num_par < MAX_PAR; m_num_par++)
		delete m_par[m_num_par];
}

int Packet::addPar(PacketPar *par)
{
	if(m_num_par < MAX_PAR)
	{
		m_par[m_num_par++] = par;
		m_length += (par->m_length +2);		
		return 0;
	}
	return -1;
}

int Packet::addIdPar(ParType par_id, uint8_t *id)
{
	m_control_word |= (par_id == ParType::STATION_ID) ? STATIONID_CW : NETWORKID_CW;
	return addPar(new IdPar(par_id, id, strlen((char *)id)));
}

int Packet::addMacSetPar(AddrSet *set, bool update_cw)
{
	if (update_cw)
		m_control_word |= (SETPROV_CW | set->slapType());
	return addPar(new MacSetPar(*set));
}

int Packet::addLifetimePar(uint16_t lifetime)
{
	return addPar(new LifetimePar(lifetime));
}

int Packet::addClientAddrPar(AddrSet *set)
{
	return addPar(new ClientAddrPar(*set));
}

int Packet::addVendorPar(uint8_t *var, uint8_t var_len)
{
	m_control_word |= VENDOR_CW;
	return addPar(new VendorPar(var, var_len));
}

int Packet::parsePar(uint8_t *&data, int &len, PacketPar *&par)
{
	ParType par_id = (ParType)*data++;
	uint8_t par_len = *data++;
	if(len < par_len)
		return -1;
	par_len -= 2;
	switch(par_id)
	{
		case ParType::STATION_ID :
		case ParType::NETWORK_ID :
			if (par_len <= 1)
				return -1;
			par = new IdPar(par_id, data, par_len);
			break;
		case ParType::MAC_ADDR_SET :
			if (par_len != 8 && par_len != 10 && par_len != 12 && par_len != 16)
				return -1;
			par = new MacSetPar(data, par_len);
			break;
		case ParType::CLIENT_ADDR : 
			if (par_len != 6 && par_len != 8)
				return -1;
			par = new ClientAddrPar(data, par_len);
			break;
		case ParType::LIFETIME :
			if (par_len != 2)
				return -1;
			par = new LifetimePar(data, par_len);
			break;
		case ParType::VENDOR :
			if (par_len <= 1)
				return -1;
			par = new VendorPar(data, par_len);
			break;
		default:
			return -1;		
	}
	data += par_len;
	len -= par_len + 2;
	return 0;
}

uint64_t Packet::get64(uint8_t *&p, uint8_t nbytes)
{
	uint64_t tmp64;

	memcpy(&tmp64, p, nbytes);
	p += nbytes;
	return(be64toh(tmp64) >> 8*(8-nbytes));
}

void Packet::set64(uint64_t val, uint8_t *&p, uint8_t nbytes)
{	
	uint64_t tmp64;

	tmp64 = htobe64(val << 8*(8-nbytes)); 
	memcpy(p, &tmp64, nbytes);
	p += nbytes;
}

uint16_t Packet::get16(uint8_t *&p)
{
	uint64_t tmp16;

	memcpy(&tmp16, p, 2);
	p += 2;
	return(be16toh(tmp16));
}

void Packet::set16(uint16_t val, uint8_t *&p)
{	
	uint16_t tmp16;

	tmp16 = htobe16(val);
	memcpy(p, &tmp16, 2);
	p += 2;
}

int Packet::parse(uint8_t *data, int len)
{
	if(len < MIN_PKT_SIZE || len > MAX_PKT_SIZE)
		return -1;

	m_DA = get64(data, 6);		
	m_SA = get64(data, 6);

	if(get16(data) != PALMA_TYPE || *data++ != PALMA_SUBTYPE)
		return -1;

	m_version = (*data & 0xE0) >> 5;
	m_message_type = (MsgType)(*data++ & 0x1F);
	m_control_word = get16(data);
	m_token = get16(data);
	m_status = (StatusCode) ((*data & 0xF0) >> 4);
	m_length = ((uint16_t)(*data++ & 0x0F)) << 8;
	m_length |= (*data++);

	len -= MIN_PKT_SIZE;
	if(len != m_length - HEADER_SIZE)
		return -1;

	for(m_num_par = 0 ; m_num_par < MAX_PAR && len >= 2; m_num_par++)
	{
		int res = parsePar(data, len, m_par[m_num_par]);
		if(res < 0)
			return res;
	}

	if(len != 0)
		return -1;
	return 0;
}

bool Packet::check()
{
	int par_cnt[(uint8_t) ParType::MAX_PARTYPE] = {0};
	for(int num_par = 0 ; num_par < m_num_par; num_par++)
	{
		uint16_t val;
		par_cnt[(uint8_t)m_par[num_par]->m_par_id]++;
		switch(m_par[num_par]->m_par_id)
		{
			case ParType::STATION_ID:
				if(!(m_control_word & STATIONID_CW))
					return false;
				break;
			case ParType::MAC_ADDR_SET:
				val = SETPROV_CW | ((MacSetPar *)(m_par[num_par]))->m_set.slapType();
				if((m_control_word & (SETPROV_CW | SLAP_CW)) != val)
					return false;
				break;
			case ParType::NETWORK_ID:
				if(!(m_control_word & NETWORKID_CW))
					return false;
				break;
			case ParType::VENDOR:
				if(!(m_control_word & VENDOR_CW))
					return false;
				break;
		}	
	}
	if(par_cnt[(uint8_t)ParType::MAC_ADDR_SET] > 2
		|| par_cnt[(uint8_t)ParType::MAC_ADDR_SET] == 2 && m_message_type != MsgType::DEFEND)
		return false;
		
	for(int i = 0; i < (uint8_t)ParType::MAX_PARTYPE; i++)
	{
		if(i != (uint8_t)ParType::MAC_ADDR_SET && par_cnt[i] > 1)
			return false;
	}
	switch(m_message_type)
	{
		case MsgType::DISCOVER:
			if((m_control_word & (SERVER_CW | NETWORKID_CW | CODEFIELD_CW | RENEWAL_CW)) 
					|| par_cnt[(uint8_t)ParType::LIFETIME] > 0)
				return false;
			break;
		case MsgType::OFFER:
			if((m_control_word & (CODEFIELD_CW | RENEWAL_CW)) 
					|| par_cnt[(uint8_t)ParType::LIFETIME] != 1 
					|| (m_control_word & (SERVER_CW | SETPROV_CW) != (SERVER_CW | SETPROV_CW)))
				return false;
			break;
		case MsgType::REQUEST:
			if((m_control_word & (SERVER_CW | NETWORKID_CW | CODEFIELD_CW)) 
					|| par_cnt[(uint8_t)ParType::LIFETIME] > 0
					|| par_cnt[(uint8_t)ParType::CLIENT_ADDR] > 0
					|| (m_control_word & (SETPROV_CW) != SETPROV_CW))
				return false;
			break;
		case MsgType::ACK:
			if((m_control_word & (RENEWAL_CW)) 
					|| (m_control_word & (SERVER_CW | CODEFIELD_CW) != (SERVER_CW | CODEFIELD_CW))
					|| par_cnt[(uint8_t)ParType::CLIENT_ADDR] > 0)
				return false;
			break;
		case MsgType::RELEASE:
			if((m_control_word & (SERVER_CW | NETWORKID_CW | CODEFIELD_CW | RENEWAL_CW))
					|| par_cnt[(uint8_t)ParType::LIFETIME] > 0 
					|| par_cnt[(uint8_t)ParType::CLIENT_ADDR] > 0
					|| (m_control_word & (SETPROV_CW) != SETPROV_CW))
				return false;
			break;
		case MsgType::DEFEND:
			if((m_control_word & (SERVER_CW | NETWORKID_CW | CODEFIELD_CW | RENEWAL_CW))
					|| par_cnt[(uint8_t)ParType::LIFETIME] != 1
					|| par_cnt[(uint8_t)ParType::CLIENT_ADDR] > 0
					|| (m_control_word & (SETPROV_CW) != SETPROV_CW)
					|| par_cnt[(uint8_t)ParType::MAC_ADDR_SET] != 2)
				return false;
			break;
		case MsgType::ANNOUNCE:
			if((m_control_word & (SERVER_CW | NETWORKID_CW | CODEFIELD_CW | RENEWAL_CW))
					|| par_cnt[(uint8_t)ParType::LIFETIME] != 1
					|| par_cnt[(uint8_t)ParType::CLIENT_ADDR] > 0
					|| (m_control_word & (SETPROV_CW) != SETPROV_CW))
				return false;
			break;
	}
	return true;
}


int Packet::toBuffer(uint8_t *data)
{
	set64(m_DA, data, 6);		
	set64(m_SA, data, 6);	
	set16(PALMA_TYPE, data);	
	*data++ = PALMA_SUBTYPE;
	*data++ = (m_version << 5) | (uint8_t) m_message_type;
	set16(m_control_word, data);
	set16(m_token, data);
	*data++ = ((uint8_t) m_status << 4) | (m_length >> 8);
	*data++ = m_length & 0xFF;

	int len = MIN_PKT_SIZE;

	for(int m_num_par = 0 ; m_num_par < MAX_PAR && m_par[m_num_par] != NULL; m_num_par++)
		len += m_par[m_num_par]->toBuffer(data);

	return len;
}

void Packet::setSA(uint64_t addr)
{
	m_SA = addr;
}

void Packet::setRenewal()
{
	m_control_word |= RENEWAL_CW;
}

bool Packet::getRenewal()
{
	return m_control_word & RENEWAL_CW;
}

MsgType Packet::getType()
{
	return m_message_type;
}

uint64_t Packet::getDA()
{
	return m_DA;
}

uint64_t Packet::getSA()
{
	return m_SA;
}

StatusCode Packet::getStatus()
{
	return m_status;
}

uint16_t Packet::getToken()
{
	return m_token;
}

AddrSet *Packet::getSet(bool first_instance)
{
	if(m_control_word & SETPROV_CW)
	{
		for(int i = 0; i < m_num_par; i++)
		{
			if(m_par[i] != NULL && (m_par[i]->m_par_id == ParType::MAC_ADDR_SET))
				if(first_instance)
					return &(((MacSetPar *)(m_par[i]))->m_set);
				else
					first_instance = true;
		}
	}
	return NULL;
}

uint64_t Packet::getClientAddr()
{
	for(int i = 0; i < m_num_par; i++)
	{
		if(m_par[i] != NULL && m_par[i]->m_par_id == ParType::CLIENT_ADDR)
			return ((ClientAddrPar *)(m_par[i]))->m_set.m_addr;
	}
	return 0;
}

uint16_t Packet::getLifetime()
{
	for(int i = 0; i < m_num_par; i++)
	{
		if(m_par[i] != NULL && m_par[i]->m_par_id == ParType::LIFETIME)
			return ((LifetimePar *)(m_par[i]))->m_lifetime;
	}
	return 0;
}

uint8_t *Packet::getStationId()
{
	if(m_control_word & STATIONID_CW)
	{
		for(int i = 0; i < m_num_par; i++)
		{
			if(m_par[i] != NULL && m_par[i]->m_par_id == ParType::STATION_ID)
				return ((IdPar *)(m_par[i]))->m_id;
		}
	}
	return NULL;
}

uint8_t *Packet::getNetworkId()
{
	if(m_control_word & NETWORKID_CW)
	{
		for(int i = 0; i < m_num_par; i++)
		{
			if(m_par[i] != NULL && m_par[i]->m_par_id == ParType::NETWORK_ID)
				return ((IdPar *)(m_par[i]))->m_id;
		}
	}
	return NULL;
}

uint8_t *Packet::getVendorVar()
{
	if(m_control_word & VENDOR_CW)
	{
		for(int i = 0; i < m_num_par; i++)
		{
			if(m_par[i] != NULL && m_par[i]->m_par_id == ParType::VENDOR)
				return ((VendorPar *)(m_par[i]))->m_var;
		}
	}
	return NULL;
}


