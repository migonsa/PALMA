#ifndef DETAILS_H
#define DETAILS_H

#include <stdlib.h>
#include "addrset.h"

#define DISCOVERY_TIMEOUT (.5 + drand48()*.1)
#define DISCOVER_DSC_COUNT 3
#define REQUESTING_TIMEOUT (.5 + drand48()*.1)
#define REQUEST_REQ_COUNT 3
#define ANNOUNCE_TIMEOUT	(30. + drand48()*2.)
#define SELF_ASSIGMENT_LIFETIME 1800 //(12.*60.*60.)

extern const uint64_t PALMA_MCAST;
extern const uint16_t PALMA_TYPE;
extern const uint8_t PALMA_SUBTYPE;
extern const AddrSet DISCOVER_SOURCE_ADDR_RANGE;
extern const AddrSet AUTOASSIGN_UNICAST;
extern const AddrSet AUTOASSIGN_MULTICAST;
extern const AddrSet AUTOASSIGN_UNICAST_64;
extern const AddrSet AUTOASSIGN_MULTICAST_64;
extern const int MAX_AUTOASSIGN_UNICAST;
#endif
