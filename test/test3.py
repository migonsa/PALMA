#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import CPULimitedHost
from mininet.topo import Topo
from mininet.log import setLogLevel, info
from mininet.util import custom, pmonitor
from mininet.node import OVSController
from mininet.nodelib import LinuxBridge
from mininet.cli import CLI


test_topo = Topo()
switch = test_topo.addSwitch( 's1' )
host = test_topo.addHost( 'server1' )
test_topo.addLink( host, switch)
for h in range( 1, 3 ):
	host = test_topo.addHost( 'h%s' % h)
	test_topo.addLink( host, switch)
net = Mininet(topo=test_topo, host=CPULimitedHost, controller = OVSController, switch = LinuxBridge)
net.start()
CLI(net)
