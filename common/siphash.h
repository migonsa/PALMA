#ifndef SIPHASH_H
#define SIPHASH_H

#include <inttypes.h>

class SipHash {
private:
		int m_idx;
		uint64_t m_k0, m_k1;
		uint64_t m_v0, m_v1, m_v2, m_v3, m_m;
		uint8_t m_input_len;
public:
		SipHash();
		void begin();
		void update(uint8_t data);
		void update(uint8_t *data);
		void update(uint16_t data);
		void update(uint64_t data);
	  	uint64_t digest();
};

#endif
