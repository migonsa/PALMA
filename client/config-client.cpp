#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/addrset.h"
#include "config-client.h"

ConfigClient::ConfigClient()
{
	m_list = new ConfigElement *[ConfigItem::MAX_CONFIG_ITEM]
	{
		new ConfigString(NULL),
		new ConfigAddr(0),
		new ConfigSet(0x0a0000000000L, (0xffffffffffL + 1)),
		new ConfigSize(1),
		new ConfigSize(16),
		new ConfigAddr(0),
		new ConfigBool(false),
		new ConfigBool(true),
		new ConfigString(NULL),
		new ConfigString(NULL),
		new ConfigBool(false),
	};
	m_root_tag = "ClientConfig";
	m_array_tags = new const char*[ConfigItem::MAX_CONFIG_ITEM]
	{
		"InterfaceName",
		"PreassignedSrcAddress",
		"ClaimAddressSet",
		"MinAssignedAddresses",
		"MaxAssignedAddresses",
		"KnownServerAddress",
		"RenewalActive",
		"RandomAutoAssign",
		"ClientStationId",
		"VendorParameter",
		"Verbose"
	};
}

ConfigClient::~ConfigClient()
{
	for(int i=0; i<ConfigItem::MAX_CONFIG_ITEM; i++)
		delete m_list[i];
	delete m_list;
	delete m_array_tags;
}

bool ConfigClient::check(const char *fname)
{
	if(TO_STRING(get(ConfigItem::INTERFACE)) == NULL)
	{
		fprintf(stderr, "%s: Missing interface name\n", fname);
		return false;
	}
	if((TO_ADDRSET_PTR(get(ConfigItem::CLAIM_SET))->getSize() > 0
		&& TO_ADDRSET_PTR(get(ConfigItem::CLAIM_SET))->getSize() < TO_SIZE(get(ConfigItem::MAX_ADDR_CLAIM)))
			|| TO_SIZE(get(ConfigItem::MIN_ADDR_CLAIM)) > TO_SIZE(get(ConfigItem::MAX_ADDR_CLAIM)))
	{
		fprintf(stderr, "%s: Incoherent sizes\n", fname);
		return false;
	}
	if(!TO_ADDR(get(ConfigItem::PREASSIGNED_ADDR)) && TO_ADDR(get(ConfigItem::KNOWN_SERVER_ADDR)))
	{
		fprintf(stderr, "%s: PreassignedSrcAddress is required for KnownServerAddress\n", fname);
		return false;
	}
	return true;
}

