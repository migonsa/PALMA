#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <memory.h>
#include <unistd.h>

#include "netitf.h"
#include "details.h"
#include "palma.h"
#include "packet.h"

NetItf::NetItf(Palma *protocol) : m_protocol(protocol){}

void NetItf::init(uint8_t *ifname)
{
	int res;
	ifreq ifbuf = {0};
	sockaddr_ll saddr = {0};

	m_fd = socket(PF_PACKET, SOCK_RAW, htons(PALMA_TYPE));
	if(m_fd < 0)
	{
		perror("Opening socket");
		exit(1);
	}
	strncpy(ifbuf.ifr_name, (char *)ifname, IFNAMSIZ);
	res = ioctl(m_fd, SIOCGIFINDEX, &ifbuf);
	if(res < 0)
	{
		perror("Getting interface index");
		exit(1);
	}
	m_ifidx = ifbuf.ifr_ifindex;
	saddr.sll_family = AF_PACKET;
	saddr.sll_ifindex = m_ifidx;
	saddr.sll_halen = ETH_ALEN;

	res = bind(m_fd, (sockaddr*)&saddr, sizeof(saddr));
	if(res != 0)
	{
		perror("Binding datagram socket");
		exit(1);
	}
}

NetItf::~NetItf()
{
	close(m_fd);
}

int NetItf::onInput()
{
	uint8_t rcvbuf[MAX_PKT_SIZE+1];
	int rcvlen;

	while ((rcvlen = recv(m_fd, rcvbuf, MAX_PKT_SIZE+1, MSG_DONTWAIT)) >= 0)
	{
		Packet pkt;											//solo para depurar
		/*		
		printf("\nRecibidos %d bytes\n",rcvlen);
		printf("\nDATA->");
		for(int i=0; i<rcvlen; i++)
			printf("%02x ",rcvbuf[i]);
		printf("\n");
		*/
		if(pkt.parse(rcvbuf, rcvlen) == 0 && pkt.check())
			m_protocol->handlePacket(&pkt);
	}
	if (errno != EAGAIN && errno != EWOULDBLOCK)
	{
		perror("Reading from network socket");
		exit(1);
	}
}

void NetItf::netsend(Packet *pkt)
{
	uint8_t sndbuf[MAX_PKT_SIZE];

	uint16_t len = pkt->toBuffer(sndbuf);
	int res = send(m_fd, sndbuf, len, 0);
	if(res	< 0)
	{
		perror("Sending packet");
		exit(1);
	}
	/*
	printf("\nEnviados %d bytes->\n",res);
	for(int i=0; i<res; i++)
		printf("%02x ",sndbuf[i]);
	printf("\n");
	*/
}

void NetItf::fillMreq(packet_mreq& mreq, uint64_t addr, bool multicast)
{
	addr = htobe64(addr);
	mreq.mr_ifindex = m_ifidx;
	mreq.mr_type = multicast ? PACKET_MR_MULTICAST : PACKET_MR_UNICAST;
	mreq.mr_alen = 6;
	memcpy(mreq.mr_address, &addr, mreq.mr_alen);
}

void NetItf::addAddr(uint64_t addr, bool multicast)
{
	packet_mreq mreq = {0};
	fillMreq(mreq, addr, multicast);

	int res = setsockopt(m_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	if(res	< 0)
	{
		perror("Adding address to filter");
		exit(1);
	}
}

void NetItf::delAddr(uint64_t addr, bool multicast)
{
	packet_mreq mreq = {0};
	fillMreq(mreq, addr, multicast);

	int res = setsockopt(m_fd, SOL_PACKET, PACKET_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
	if(res	< 0)
	{
		perror("Dropping address to filter");
		exit(1);
	}
}

