#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import CPULimitedHost
from mininet.topo import SingleSwitchTopo
from mininet.log import setLogLevel, info
from mininet.util import custom, pmonitor
from mininet.node import OVSController
from mininet.cli import CLI
import time

def monitorhosts( hosts=5, sched='cfs' ):
	"Start a bunch of clients"
	mytopo = SingleSwitchTopo( hosts )
	cpu = .9 / hosts
	myhost = custom( CPULimitedHost, cpu=cpu, sched=sched )
	net = Mininet(topo=mytopo, host=myhost, controller = OVSController)
	net.start()
	net["s1"].sendCmd("wireshark -i s1-eth1 -Y palma -k")
	time.sleep(3)
	popens = {}
	i = 0
	for host in net.hosts:
		popens[ host ] = host.popen( "sudo ../client/palma-client -c ../configs/client.xml -i %s-eth0 -s %s -p 0x100bbb%x00%x11" % (host.name, host.name.upper(), i, i))
		i += 1
	for host, line in pmonitor( popens ):
		if host:
			print( "<%s>: %s" % ( host.name, line ) )
	net.stop()

if __name__ == '__main__':
	setLogLevel( 'info' )
	monitorhosts( hosts=10 )
