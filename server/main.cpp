#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 
#include <unistd.h>

#include "palma-server.h"
#include "../common/packet.h"
#include "config-server.h"


int main(int argc, char *argv[])
{
	// Procesa opciones
	int c;
	char *itfname = NULL;
	char *confname = NULL;

	while ((c = getopt (argc, argv, "c:i:")) != -1)
	{
		switch (c)
		{
			case 'c':
				confname = optarg;
				break;
			case 'i':
				itfname = optarg;
				break;
			case '?':
				fprintf(stderr,"Invalid option.\n");
				fprintf(stderr,"Uso:%s [-c <config filename>][-i <interface name>]\n", argv[0]);
				exit(1);
			default:
				abort();
		}
	}
	if(optind != argc)
	{
		fprintf(stderr,"Invalid arguments.\n");
		fprintf(stderr,"Uso:%s [-c <config filename>][-i <interface name>]\n", argv[0]);
		exit(1);
	}
	
	PalmaServer::initRandom();
	PalmaServer server;
	server.m_config.set(ConfigItem::INTERFACE, itfname);

	if(confname && !server.m_config.read(confname))
	{
		fprintf(stderr, "Invalid configuration in: %s\n", confname);
		exit(1);
	}

	server.begin();
}
