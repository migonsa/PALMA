#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 
#include <unistd.h>

#include "palma-client.h"
#include "../common/packet.h"
#include "config-client.h" 


int main(int argc, char *argv[])
{
	// Procesa opciones
	int c;
	char *itfname = NULL;
	char *confname = NULL;
	char *station_id = NULL;
	uint64_t preassigned_addr = 0;

	while ((c = getopt (argc, argv, "c:i:s:p:")) != -1)
	{
		switch (c)
		{
			case 'c':
				confname = optarg;
				break;
			case 'i':
				itfname = optarg;
				break;
			case 's':
				station_id = optarg;
				break;
			case 'p':
  				if(!Config::toUint64(optarg, preassigned_addr))
				{
					fprintf(stderr,"Invalid address value.\n");
					exit(1);
				}
				break;
			case '?':
				fprintf(stderr,"Invalid option.\n");
				fprintf(stderr,	"Uso:%s [-c <config filename>][-i <interface name>][-s <station id>][-p <preassigned address>]\n", argv[0]);
				exit(1);
			default:
				abort();
		}
	}
	if(optind != argc)
	{
		fprintf(stderr,"Invalid arguments.\n");
		fprintf(stderr,	"Uso:%s [-c <config filename>][-i <interface name>][-s <station id>][-p <preassigned address>]\n", argv[0]);
		exit(1);
	}	

	PalmaClient::initRandom();
	PalmaClient client;
	client.m_config.set(ConfigItem::INTERFACE, itfname);
	client.m_config.set(ConfigItem::STATION_ID, station_id);
	client.m_config.set(ConfigItem::PREASSIGNED_ADDR, &preassigned_addr);

	if(confname && !client.m_config.read(confname))
	{
		fprintf(stderr, "Invalid configuration in: %s\n", confname);
		exit(1);
	}
	client.begin();
}
