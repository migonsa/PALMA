#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/addrset.h"
#include "config-server.h"

ConfigServer::ConfigServer()
{
	m_list = new ConfigElement *[ConfigItem::MAX_CONFIG_ITEM]
	{
		new ConfigString(NULL),
		new ConfigAddr(0),
		new ConfigSet(0x000000000000, 0),
		new ConfigSet(0x000000000000, 0),
		new ConfigSet(0x0000000000000000, 0),
		new ConfigSet(0x0000000000000000, 0),
		new ConfigSize(1),
		new ConfigSize(0),
		new ConfigSize(0),
		new ConfigSize(0),
		new ConfigSize(1),
		new ConfigInt(60),
		new ConfigInt(60),
		new ConfigInt(60),
		new ConfigInt(60),
		new ConfigInt(3),
		new ConfigBool(false),
		new ConfigBool(false),
		new ConfigBool(false),
		new ConfigBool(true),
		new ConfigBool(true),
		new ConfigString(NULL),
		new ConfigString(NULL),
	};
	m_root_tag = "ServerConfig";
	m_array_tags = new const char*[ConfigItem::MAX_CONFIG_ITEM]
	{
		"InterfaceName",
		"SrcAddress",
		"UnicastAddressSet",
		"MulticastAddressSet",
		"Unicast64AddressSet",
		"Multicast64AddressSet",
		"MaxAssignedUnicast",
		"MaxAssignedMulticast",
		"MaxAssignedUnicast64",
		"MaxAssignedMulticast64",
		"MaxAssignedDefault",
		"UnicastLifetime",
		"MulticastLifetime",
		"Unicast64Lifetime",
		"Multicast64Lifetime",
		"ReserveLifetime",
		"RenewalActive",
		"DefaultMulticast",
		"Default64bitSet",
		"AutoassignObjectionActive",
		"AlternateSetActive",
		"NetworkId",
		"VendorParameter",
	};
}

ConfigServer::~ConfigServer()
{
	for(int i=0; i<ConfigItem::MAX_CONFIG_ITEM; i++)
		delete m_list[i];
	delete m_list;
	delete m_array_tags;
}

bool ConfigServer::check(const char *fname)
{
	if(TO_STRING(get(ConfigItem::INTERFACE)) == NULL)
	{
		fprintf(stderr, "%s: Missing interface name\n", fname);
		return false;
	}
	if(!TO_ADDR(get(ConfigItem::SRC_ADDR)))
	{
		fprintf(stderr, "%s: SrcAddress is required\n", fname);
		return false;
	}
	if(TO_ADDRSET_PTR(get(ConfigItem::UNICAST_SET))->getSize() <= 0)
	{
		fprintf(stderr, "%s: Missing Unicast set for distribution\n", fname);
		return false;
	}
	if(TO_ADDRSET_PTR(get(ConfigItem::UNICAST_SET))->getSize() < TO_UINT(get(ConfigItem::MAX_ADDR_UNICAST)))
	{
		fprintf(stderr, "%s: Incoherent size of MaxAssignedUnicast\n", fname);
		return false;
	}
	if(TO_ADDRSET_PTR(get(ConfigItem::MULTICAST_SET))->getSize() < TO_UINT(get(ConfigItem::MAX_ADDR_MULTICAST)))
	{
		fprintf(stderr, "%s: Incoherent size of MaxAssignedMulticast\n", fname);
		return false;
	}
	if(TO_ADDRSET_PTR(get(ConfigItem::UNICAST_64_SET))->getSize() < TO_UINT(get(ConfigItem::MAX_ADDR_UNICAST_64)))
	{
		fprintf(stderr, "%s: Incoherent size of MaxAssignedUnicast64\n", fname);
		return false;
	}
	if(TO_ADDRSET_PTR(get(ConfigItem::MULTICAST_64_SET))->getSize() < TO_UINT(get(ConfigItem::MAX_ADDR_MULTICAST_64)))
	{
		fprintf(stderr, "%s: Incoherent size of MaxAssignedMulticast64\n", fname);
		return false;
	}
	if(TO_BOOL(get(ConfigItem::DEFAULT_MULTICAST)))
	{
		if(TO_BOOL(get(ConfigItem::DEFAULT_64)))
		{
			if(TO_ADDRSET_PTR(get(ConfigItem::MULTICAST_64_SET))->getSize() <= 0)
			{
				fprintf(stderr, "%s: Missing Multicast-64 set for distribution\n", fname);
				return false;
			}
			if(TO_ADDRSET_PTR(get(ConfigItem::MULTICAST_64_SET))->getSize() < TO_UINT(get(ConfigItem::DEFAULT_ADDR_OFFER)))
			{
				fprintf(stderr, "%s: Incoherent size of MaxAssignedDefault\n", fname);
				return false;
			}
		}
		else
		{
			if(TO_ADDRSET_PTR(get(ConfigItem::MULTICAST_SET))->getSize() <= 0)
			{
				fprintf(stderr, "%s: Missing Multicast set for distribution\n", fname);
				return false;
			}
			if(TO_ADDRSET_PTR(get(ConfigItem::MULTICAST_SET))->getSize() < TO_UINT(get(ConfigItem::DEFAULT_ADDR_OFFER)))
			{
				fprintf(stderr, "%s: Incoherent size of MaxAssignedDefault\n", fname);
				return false;
			}
		}
	}
	else
	{
		if(TO_BOOL(get(ConfigItem::DEFAULT_64)))
		{
			if(TO_ADDRSET_PTR(get(ConfigItem::UNICAST_64_SET))->getSize() <= 0)
			{
				fprintf(stderr, "%s: Missing Unicast-64 set for distribution\n", fname);
				return false;
			}
			if(TO_ADDRSET_PTR(get(ConfigItem::UNICAST_64_SET))->getSize() < TO_UINT(get(ConfigItem::DEFAULT_ADDR_OFFER)))
			{
				fprintf(stderr, "%s: Incoherent size of MaxAssignedDefault\n", fname);
				return false;
			}
		}
		else
		{
			if(TO_ADDRSET_PTR(get(ConfigItem::UNICAST_SET))->getSize() < TO_UINT(get(ConfigItem::DEFAULT_ADDR_OFFER)))
			{
				fprintf(stderr, "%s: Incoherent size of MaxAssignedDefault\n", fname);
				return false;
			}
		}
	}
	return true;
}

