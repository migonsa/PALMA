#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include "addrset.h"

#define ETH_HDR_SIZE	(2*6 + 2)
#define HEADER_SIZE		8
#define MIN_PKT_SIZE	(ETH_HDR_SIZE + HEADER_SIZE)
#define MAX_PKT_SIZE	(MIN_PKT_SIZE+4+18+2*255+10+255) /* Fix part + Lifetime + MacAddrSet +  2 x id + ClientAddr + Vendor */
#define MAX_PAR		6

enum class MsgType : uint8_t
{
	DISCOVER 		= 1, 
	OFFER			= 2,
	REQUEST 		= 3,
	ACK				= 4, 
	RELEASE 		= 5,
	DEFEND			= 6,
	ANNOUNCE		= 7, 
};

enum class ParType : uint8_t
{
	STATION_ID		= 1,
	MAC_ADDR_SET 	= 2,
	NETWORK_ID 		= 3,
	LIFETIME   		= 4,
	CLIENT_ADDR		= 5,
	VENDOR			= 6,
	MAX_PARTYPE		= 7
};

enum class StatusCode : uint8_t
{
	NO_CODE			= 0,
	ASSIGN_OK		= 1,
	ALTERNATE_SET	= 2,
	FAIL_CONFLICT	= 3,
	FAIL_DISALLOWED	= 4,
	FAIL_TOO_LARGE	= 5,
	FAIL_OTHER		= 6,
};

class PacketPar
{
public:
	ParType m_par_id;
	uint8_t m_length;

	PacketPar(ParType par_id, uint8_t len);
	virtual ~PacketPar() {};
	virtual int toBuffer(uint8_t *&data);
	virtual PacketPar* clone() = 0;
};

class IdPar : public PacketPar
{
public:
	uint8_t *m_id;

	IdPar(ParType par_id, uint8_t *data, uint8_t len);	
	~IdPar();
	int toBuffer(uint8_t *&data);
	PacketPar* clone();
};

class MacSetPar : public PacketPar
{
public:
	AddrSet m_set;

	MacSetPar(uint8_t *data, uint8_t len);
	MacSetPar(AddrSet &set);
	~MacSetPar() {}
	int toBuffer(uint8_t *&data);	
	uint8_t length(AddrSet &set);
	PacketPar* clone();
};


class LifetimePar : public PacketPar
{
public:
	uint16_t m_lifetime;

	LifetimePar(uint8_t *data, uint8_t len);
	LifetimePar(uint16_t lifetime);
	~LifetimePar() {}
	int toBuffer(uint8_t *&data);
	PacketPar* clone();
};

class ClientAddrPar : public PacketPar
{
public:
	AddrSet m_set;

	ClientAddrPar(uint8_t *data, uint8_t len);
	ClientAddrPar(AddrSet &set);
	~ClientAddrPar() {}
	int toBuffer(uint8_t *&data);
	PacketPar* clone();
};

class VendorPar : public PacketPar
{
public:
	uint8_t *m_var;

	VendorPar(uint8_t *data, uint8_t var_len);	
	~VendorPar();
	int toBuffer(uint8_t *&data);
	PacketPar* clone();
};

class Packet
{
	uint64_t m_DA;
	uint64_t m_SA;
	uint8_t m_version;
	MsgType m_message_type;
	uint16_t m_control_word;
	uint16_t m_token;
	StatusCode m_status;
	uint16_t m_length;
	uint8_t m_num_par;
	PacketPar *m_par[MAX_PAR];

public:

	Packet();
	Packet(Packet *pkt);
	Packet(MsgType type, uint64_t DA, uint64_t SA, uint16_t token, StatusCode status = StatusCode::NO_CODE);
	~Packet();
	int addPar(PacketPar *par);
	int addIdPar(ParType par_id, uint8_t *id);
	int addMacSetPar(AddrSet *set, bool update_cw = true);
	int addLifetimePar(uint16_t lifetime);
	int addVendorPar(uint8_t *var, uint8_t var_len);
	int addClientAddrPar(AddrSet *set);
	int parsePar(uint8_t *&data, int &len, PacketPar *&Par);
	static uint64_t get64(uint8_t *&p, uint8_t nbytes = 8);
	static void set64(uint64_t val, uint8_t *&p,  uint8_t nbytes = 8);
	static uint16_t get16(uint8_t *&p);
	static void set16(uint16_t val, uint8_t *&p);
	int parse(uint8_t *data, int len);
	bool check();
	int toBuffer(uint8_t *data);
	void setSA(uint64_t addr);
	void setRenewal();
	bool getRenewal();
	MsgType getType();
	uint64_t getDA();
	uint64_t getSA();
	uint16_t getToken();
	StatusCode getStatus();
	AddrSet *getSet(bool first_instance = true);
	uint64_t getClientAddr();
	uint16_t getLifetime();
	uint8_t *getStationId();
	uint8_t *getNetworkId();
	uint8_t *getVendorVar();
};

#endif
