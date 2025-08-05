#!/usr/bin/python

from mininet.net import Mininet
from mininet.node import CPULimitedHost
from mininet.topo import Topo
from mininet.log import setLogLevel, info
from mininet.util import custom, pmonitor
from mininet.node import OVSController
from mininet.cli import CLI
from datetime import datetime, timedelta
import random
import signal
import sys
import math

class TimerList:	
	def __init__(self):
		self.list = []
		self.ref = datetime.now()

	def refresh(self):
		new_ref = datetime.now()
		if(len(self.list) != 0):
			self.list[0][0] -= (new_ref - self.ref)
		self.ref = new_ref

	def insert(self, time, obj, func):
		self.refresh()
		idx = 0
		t = timedelta(seconds=time)
		for idx in range(len(self.list)):
			if(t < self.list[idx][0]):
				self.list[idx][0] -= t 
				self.list.insert(idx,[t, obj, func])				
				idx = None
				break
			t -= self.list[idx][0]
		if(idx != None):
			self.list.append([t, obj, func])
			
	def check(self):
		self.refresh()
		while(True):
			elem = self.list[0]
			if(elem[0] <= timedelta()):
				self.list.pop(0)
				elem[2](elem[1],elem[0])
			else:
				break
		#print("check : %f\n" % (self.list[0][0].total_seconds()/timedelta(microseconds=1000).total_seconds()))
		#print(self.list)
		return self.list[0][0].total_seconds()/timedelta(microseconds=1000).total_seconds()

class Test:
	def __init__(self, n_clients, n_servers=1, n_ports = 64, lambda_in=10, lambda_out=1./10, \
					test_time=-1, output=None):
		self.popens = {}
		self.timer_list = TimerList()
		self.servers = []
		self.idle_clients = []
		self.finalize = False
		self.lambda_in = lambda_in
		self.lambda_out = lambda_out

		def signal_handler(sig, frame):
			self.finalize = True

		signal.signal(signal.SIGINT, signal_handler)
 
		test_topo = Topo()

		def genTree(childs,level):
			switches = []
			n_switches = 1
			n_childs = len(childs)
			if(n_childs > n_ports):
				n_switches = int(math.ceil(n_childs/(n_ports-1.)))
			for s in range(1, n_switches+1):
				switches.append(test_topo.addSwitch( 's%d_%s' % (level, s)))
			while(len(childs)):
				for s in switches:
					test_topo.addLink(childs.pop(0), s)
			if(n_switches > 2):
				return(genTree(switches,level+1))
			if(n_switches == 2):
				test_topo.addLink( switches[0], switches[1])
			return

		srvs = []
		clnts = []
		for h in range( 1, n_servers+1 ):
			srvs.append(test_topo.addHost( 'srv%s' % h ))
		for h in range( 1, n_clients+1 ):
			clnts.append(test_topo.addHost( 'h%s' % h ))

		genTree(srvs+clnts, 0)

		self.net = Mininet(topo=test_topo, host=CPULimitedHost, controller = OVSController)
		#s1.sendcmd("wireshark");
		self.net.start()
		for h in range( 1, n_servers+1 ):
			self.servers.append(self.net['srv%s' % h])
		for h in range( 1, n_clients+1 ):
			client = self.net['h%s' % h]
			client.setCPUFrac(.5/n_clients)
			self.idle_clients.append(client)
			
		self.timer_list.insert(random.expovariate(lambda_in), None, self.arrive)
		if(test_time > 0):
			self.timer_list.insert(test_time, None, self.finish)
		if(output != None):
			self.file = open(output,"w")
		else:
			self.file = sys.stdout
		self.file.write("HOST,TIME,CMD,ADDR,COUNT\n")

	def run(self):
		for server in self.servers:
			self.popens[server] = server.popen(\
				"sudo ../client/palma-server -c ../configs/%s.xml -i %s-eth0" \
					% (server.name, server.name))

		while(not self.finalize):
			try:
				host, line = pmonitor(self.popens, timeoutms=self.timer_list.check()).next()
				if(host):
					self.file.write("%s,%s" % (host, line))
			except StopIteration:
				pass			
		self.file.close()
		self.net.stop()

	'''def writeLine(self, host, line):
		data = line.split(': ', 1)
		time = data[0]
		data = data[1].split('[', 1)
		command = data[0]
		if(len(data) > 1):
			parameters = data[1].split(']')
		else:
			parameters = ""
		self.file.write("%s, %s, %s, %s\n" % (host, time, command, parameters))'''
			
	def arrive(self, obj, t):
		if(len(self.idle_clients) > 0):
			new_client = self.idle_clients.pop(0)
			self.popens[new_client] = new_client.popen(\
				"sudo ../client/palma-client -c ../configs/client.xml -i %s-eth0 -s %s"\
					 % (new_client.name, new_client.name.upper()))
			self.timer_list.insert(random.expovariate(self.lambda_out), new_client, self.departure)
		
		self.timer_list.insert(random.expovariate(self.lambda_in), None, self.arrive)

	def departure(self, client, t):
		self.popens[client].send_signal(signal.SIGTERM)
		self.idle_clients.append(client)

	def finish(self, obj, t):
		self.finalize = True
		
	
if __name__ == '__main__':
	setLogLevel( 'info' )
	Test(100, 0, test_time=60, output="results.csv", lambda_in=20, lambda_out=1./15).run()
