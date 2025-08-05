#include "details.h"

const uint64_t PALMA_MCAST = 0x0180c2abcdef;
const uint16_t PALMA_TYPE = 0x33ff;
const uint8_t PALMA_SUBTYPE = 0x00;

const AddrSet DISCOVER_SOURCE_ADDR_RANGE(0x2a0000000000L,0xffffffffL + 1);
const AddrSet AUTOASSIGN_UNICAST(0x0a0000000000L,0xffffffffffL + 1);
const AddrSet AUTOASSIGN_MULTICAST(0x0b0000000000L,0xffffffffffL + 1);
const AddrSet AUTOASSIGN_UNICAST_64(0x0a00000000000000L,0xffffffffffffffL + 1);
const AddrSet AUTOASSIGN_MULTICAST_64(0x0b00000000000000L,0xffffffffffffffL + 1);
const int MAX_AUTOASSIGN_UNICAST = 16;
