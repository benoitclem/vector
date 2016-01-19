#!/usr/bin/python
# coding: utf8

import json
import os

class dataHandler:
	def __init__(self,name):
		self.fname = name+'.json'
		if not self.repoExists():
			self.commitData({}) # Empty 

	def repoExists(self):
		return os.path.exists(self.fname)

	def getData(self):
		f = open(self.fname,'r')
		d = f.read()
		f.close()
		return json.loads(d)

	def commitData(self,dic):
		#print("commitData")
		jData = json.dumps(dic)
		f = open(self.fname, 'w')
		f.write(jData)
		f.close()

	def clientExists(self,mac):
		d = self.getData()
		try:
			d[mac]
			return True
		except KeyError:
			return False

	def listClients(self):
		clients = self.getData()
		clientsKeys = clients.keys()
		ret = []
		for client in clientsKeys:
			ret.append(client+'['+clients[client]["name"]+']')
		return ret

	def getClient(self, client):
		clients = self.getData()
		print(clients)
		return clients[client]

	def addClient(self, mac, name = "unknown", group = -1, quote = "empty message", message = "no message", brief = "brief"):
		d = self.getData()
		d[mac] = {'name': name, 'group': group, 'quote': quote, 'message': message, 'messageStatus':1, 'brief': brief}
		self.commitData(d)

	def get(self, what, mac):
		d = self.getData()
		try:
			ret = d[mac][what]
		except KeyError:
			ret = None
		return ret

	def ackMessage(self,mac):
		d = self.getData()
		try:
			d[mac]['messageStatus'] = 1
		except KeyError:
			pass
		self.commitData(d)

	def updateClient(self,mac,message,quote,briefs):
		d = self.getData()
		try:
			if message != None:
				d[mac]['messageStatus'] = 2
				d[mac]['message'] = message		
			if quote != None:
				d[mac]['quote'] = quote
			if briefs != None:
				d[mac]['brief'] = briefs
		except KeyError:
			pass
		self.commitData(d)