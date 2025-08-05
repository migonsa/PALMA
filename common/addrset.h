#ifndef SET_H
#define SET_H

#include <stdint.h>

enum class SetType : uint8_t
{
	AUTO = 0,
	ADDR = 1,
	MASK = 2,
};

enum class SetSize : uint8_t
{
	AUTO = 0,
	SIZE48 = 6,
	SIZE64 = 8,
};

/* Same bit structure as control word */

#define AAI		(1<<0)
#define ELI		(1<<1)
#define SAI		(1<<2)
#define SZ64	(1<<4)
#define MCST	(1<<5)



class AddrSet
{
public:
	uint64_t m_addr;
	uint64_t m_val;
	SetType m_type;
	SetSize m_size;

	AddrSet(uint64_t addr=0, uint64_t count=1, SetSize size = SetSize::AUTO, SetType type = SetType::AUTO);
	uint8_t slapBits();
	uint8_t slapType();
	bool isELI();
	bool isSAI();
	bool isAAI();
	bool isLocal();
	bool isMulticast();
	bool isSize48();
	bool isSize64();
	uint8_t addrLen();
	uint64_t getAlignedMask();
	void alignToMask(SetType type);
	bool checkConflict(const AddrSet *Set1, const AddrSet *Set2);
	uint64_t getFirstAddr();
	uint64_t getLastAddr();
	uint64_t getMask();
	SetType getType();
	uint64_t getSize();
	uint64_t getAlignedSize();
	void setSize(uint64_t new_size);
};

class RandomAddrSet : public AddrSet
{
public:
	RandomAddrSet(const AddrSet& addr, uint64_t count=1);
};

#endif

