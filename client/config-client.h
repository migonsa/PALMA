#ifndef CONFIG_CLIENT_H
#define CONFIG_CLIENT_H

#include "../common/config.h"

enum ConfigItem
{
	INTERFACE,
	PREASSIGNED_ADDR,
	CLAIM_SET,
	MIN_ADDR_CLAIM,
	MAX_ADDR_CLAIM,
	KNOWN_SERVER_ADDR,
	RENEWAL,
	RANDOM_ASSIGN,
	STATION_ID,
	VENDOR,
	VERBOSE,
	MAX_CONFIG_ITEM,
};


class ConfigClient : public Config
{
public:
	ConfigClient();
	~ConfigClient();
	int getMaxItem() {return MAX_CONFIG_ITEM;}
	bool check(const char *fname);
};

#endif
