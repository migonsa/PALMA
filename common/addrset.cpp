#include <stdlib.h>
#include "addrset.h"

#define Z_BIT	(1<<3)
#define Y_BIT	(1<<2)
#define X_BIT	(1<<1)
#define M_BIT	(1<<0)

AddrSet::AddrSet(uint64_t addr, uint64_t val, SetSize size, SetType type)
{
	m_addr = addr;
	m_val = val;
	m_type = type != SetType::AUTO ? type : SetType::ADDR;
	m_size = size != SetSize::AUTO ? size : (addr > 0xffffffffffff) ? SetSize::SIZE64 : SetSize::SIZE48;
	if (m_type == SetType::MASK)
		m_addr &= m_val;	
	if (m_size == SetSize::SIZE48)
	{
		m_addr &= 0x0000FFFFFFFFFFFF;
		if (m_type == SetType::MASK)
			m_val |= 0xFFFF000000000000;
	}
	
}

uint8_t AddrSet::slapBits()
{
	switch(m_size)
	{
		case SetSize::SIZE48:
			return (uint8_t)(m_addr >> 40) & 0xF;
		case SetSize::SIZE64:
			return (uint8_t)(m_addr >> 56) & 0xF;
	}
	return 0;
}

uint8_t AddrSet::slapType()
{
	uint8_t res = isELI() ? ELI : isSAI() ? SAI : isAAI() ? AAI : 0;
	res |= isSize64() ? SZ64 : 0;
	res |= isMulticast() ? MCST : 0;
}

bool AddrSet::isELI()
{
	return ((slapBits() & (Y_BIT|Z_BIT|X_BIT)) == (Z_BIT|X_BIT));
}

bool AddrSet::isSAI()
{
	return ((slapBits() & (Y_BIT|Z_BIT|X_BIT)) == (Y_BIT|Z_BIT|X_BIT));
}

bool AddrSet::isAAI()
{
	return ((slapBits() & (Y_BIT|Z_BIT|X_BIT)) == X_BIT);
}

bool AddrSet::isLocal()
{
	return ((slapBits() & X_BIT) == X_BIT);
}

bool AddrSet::isMulticast()
{
	return ((slapBits() & M_BIT) == M_BIT);
}

bool AddrSet::isSize48()
{
	return (m_size == SetSize::SIZE48);
}

bool AddrSet::isSize64()
{
	return (m_size == SetSize::SIZE64);
}

uint8_t AddrSet::addrLen()
{
	return (uint8_t) m_size;
}


static void convertToMask(uint64_t &addr, uint64_t &val)
{
	uint64_t aux = addr + val - 1;
	val = 0xFFFFFFFFFFFFFFFF;
	aux ^= ~addr;
	for (int i=0; i<64 && (aux & (1L<<63)); i++)
	{
		aux <<= 1;
		val >>= 1;
	}
	val = ~val;
	addr &= val;
}

static void convertToAddr(uint64_t &val)
{
	uint64_t aux = ~val;
	uint64_t count = 1;
	for (int i=0; i<64 && (aux & 1); i++)
	{
		aux >>= 1;
		count <<= 1;
	}
	val = count;
}

uint64_t AddrSet::getAlignedMask()
{
	uint64_t mask, first, last, new_mask;
	mask = 0xffffffffffffffff;
	first = getFirstAddr();
	last = getLastAddr();
	while(first >= getFirstAddr() && last <= getLastAddr())
	{
		new_mask = mask;
		mask <<= 1;
		first = (first + ~mask) & mask;
		last = first + ~mask;
	}
	return new_mask;
}

void AddrSet::alignToMask(SetType type)
{
	if(m_type == SetType::MASK)
	{
		if(type == SetType::ADDR)
		{
			convertToAddr(m_val);
			m_type = type;
		}
		return;
	}
	m_val = getAlignedMask();
	m_addr = (m_addr + ~m_val) & m_val;
	if(type == SetType::MASK)
		m_type = type;
	else
		m_val = ~m_val + 1;
}


bool AddrSet::checkConflict(const AddrSet *set1, const AddrSet *set2)
{
	if(set1 == NULL || set2 == NULL)
		return false;
	uint64_t addr1 = set1->m_addr;
	uint64_t val1 = set1->m_val;
	uint64_t addr2 = set2->m_addr;
	uint64_t val2 = set2->m_val;
	uint64_t aux;
	if(set1->m_size != set2->m_size)
		return false;
	if(set1->m_type == SetType::MASK)
		convertToAddr(val1);
	if(set2->m_type == SetType::MASK)
		convertToAddr(val2);
	if(val1 == 0 || val2 == 0)
		return false;
	m_type = SetType::ADDR;
	m_size = set1->m_size;
	m_addr = (addr1 > addr2) ? addr1 : addr2;
	m_val = (addr1 + val1 < addr2 + val2) ? addr1 + val1 : addr2 + val2;
	if(m_addr >= m_val)
		return false;
	m_val -= m_addr;
	return true;	
}

uint64_t AddrSet::getFirstAddr()
{
	return m_addr;
}

uint64_t AddrSet::getLastAddr()
{
	return m_addr + getSize() - 1;
}

uint64_t AddrSet::getMask()
{
	if(m_type == SetType::MASK)
		return m_val;
	else
		return ~(getSize() - 1);
}

SetType AddrSet::getType()
{
	return m_type;
}

uint64_t AddrSet::getSize()
{
	uint64_t count = m_val;
	if(m_type == SetType::MASK)
	 	convertToAddr(count);
	return count;
}

uint64_t AddrSet::getAlignedSize()
{
	if(m_type == SetType::MASK)
		return getSize();
	return	(~getAlignedMask() + 1);
}

void AddrSet::setSize(uint64_t new_size)
{
	if(m_type == SetType::MASK)
		m_type = SetType::ADDR;
	m_val = new_size;
}

RandomAddrSet::RandomAddrSet(const AddrSet& set, uint64_t val)
{
	uint64_t count = ((AddrSet)set).getSize();
	if(val > count)
		val = count;
	m_type = SetType::ADDR;
	m_size = set.m_size;
	uint64_t random = (lrand48() ^ ((uint64_t)lrand48() << 32));
	if(val > 0xffff)
	{
		m_val = val;
		uint64_t mask = getAlignedMask();
		AddrSet pool = set;
		pool.alignToMask(SetType::MASK);
		random &= (mask ^ pool.getMask());
		m_addr = pool.getFirstAddr() + random;
		m_type = SetType::MASK;
		m_val = mask;
	}
	else
	{
		m_addr = ((AddrSet)set).getFirstAddr() + (count <= val ? 0 : (random % (count - val)));
		m_val = val;
	}
}

