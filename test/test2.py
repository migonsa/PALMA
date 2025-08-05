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
	#s1.cmd("wireshark");
	net.start()
	net["s1"].sendCmd("wireshark -i s1-eth1 -Y palma -k")
	time.sleep(3)
	#net.startTerms()
	popens = {}
	
	for host in net.hosts:
		if(host == net.hosts[0]):
			popens[ host ] = host.popen( "sudo ../server/palma-server -c ../configs/server.xml -i %s-eth0" % host.name)
		else:
			popens[ host ] = host.popen( "sudo ../client/palma-client -c ../configs/test2.xml -i %s-eth0 -n %s" % (host.name, host.name.upper()))
	
	for host, line in pmonitor( popens ):
		if host:
			info( "<%s>: %s" % ( host.name, line ) )
	
	CLI(net)
	net.stop()

if __name__ == '__main__':
	setLogLevel( 'info' )
	monitorhosts( hosts=200 )
