#include <string.h>
#include <stdlib.h>
#include "siphash.h"

#define ROTATE_LEFT(x, b) (unsigned long)(((x) << (b)) | ((x) >> (64 - (b))))

#define COMPRESS                \
  m_v0 += m_v1;                     \
  m_v2 += m_v3;                     \
  m_v1 = ROTATE_LEFT(m_v1, 13);     \
  m_v3 = ROTATE_LEFT(m_v3, 16);     \
  m_v1 ^= m_v0;                     \
  m_v3 ^= m_v2;                     \
  m_v0 = ROTATE_LEFT(m_v0, 32);     \
  m_v2 += m_v1;                     \
  m_v0 += m_v3;                     \
  m_v1 = ROTATE_LEFT(m_v1, 17);     \
  m_v3 = ROTATE_LEFT(m_v3, 21);     \
  m_v1 ^= m_v2;                     \
  m_v3 ^= m_v0;                     \
  m_v2 = ROTATE_LEFT(m_v2, 32);

#define DIGEST_BLOCK            \
  m_v3 ^= m_m;                      \
  do {                          \
    int i;                      \
    for(i = 0; i < 2; i++){     \
      COMPRESS                  \
    }                           \
  } while (0);                  \
  m_v0 ^= m_m;

SipHash::SipHash() 
{
	m_k0 = lrand48() ^ ((uint64_t)lrand48() << 32);
	m_k1 = lrand48() ^ ((uint64_t)lrand48() << 32);
}

void SipHash::begin()
{
	m_v0 = (0x736f6d6570736575 ^ m_k0);
	m_v1 = (0x646f72616e646f6d ^ m_k1);
	m_v2 = (0x6c7967656e657261 ^ m_k0);
	m_v3 = (0x7465646279746573 ^ m_k1);

	m_idx = 0;
	m_input_len = 0;
	m_m = 0;
}

void SipHash::update(uint8_t b) 
{
	m_input_len++;
	m_m |= (((uint64_t) b & 0xff) << (m_idx++ * 8));
	if (m_idx >= 8) 
	{
		DIGEST_BLOCK
		m_idx = 0;
		m_m = 0;
	}
}

void SipHash::update(uint8_t *data) 
{
	while (data != NULL && *data != '\0') 
	{
			update(*data++);
	}
}

void SipHash::update(uint16_t data)
{
	update((uint8_t)(data & 0xff));
	update((uint8_t)(data >> 8));
}

void SipHash::update(uint64_t data)
{
	for(int i = 0; i < 4; i++)
		update((uint16_t) ((data << (8*i)) & 0xffff));
}

uint64_t SipHash::digest() 
{
	while (m_idx < 7) 
	{
			m_m |= 0 << (m_idx++ * 8);
	}

	m_m |= ((uint64_t) m_input_len) << (m_idx * 8);

	DIGEST_BLOCK

	m_v2 ^= 0xff;

	for(int i = 0; i < 4; i++)
	{
			COMPRESS
	}

	return ((uint64_t) m_v0 ^ m_v1 ^ m_v2 ^ m_v3);
}
