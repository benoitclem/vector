#!/usr/bin/python
# coding: utf8

import SocketServer
import urllib2
import json
from struct import pack 
from datetime import datetime
from dataHandler import dataHandler
import socket
import smtplib
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText
from threading import Thread
from Queue import Queue
from Queue import Empty

q = Queue()
# timeout in seconds
timeout = 4
socket.setdefaulttimeout(timeout)
lastWeather = ""

def splitSentence(maxLines,maxSz,fontSpace,text):
	sText = text.split(" ")
	oText = ""
	lines = 0;
	l = 0
	i = 0
	while i < len(sText):
		if ((l + (len(sText[i]) * fontSpace)) > maxSz):
			oText = oText.strip()
			oText += ":"
			l = 0;
			lines += 1
			if(lines > maxLines):
			    oText += ";"
			    lines = 0;
		else:
			oText += sText[i] + " "
			l += (len(sText[i])+1) * fontSpace 
			i += 1
			if i == len(sText):
			    oText = oText.strip()+":"
	return oText+";"

def sendMail(text):
	msg=MIMEMultipart()
	msg['From']='zagett notifier'
	msg['Subject']='Notification de nouveau brief'
	msg.attach(MIMEText(text))

	fromaddr = 'zagett.new.year@gmail.com'
	toaddrs  = 'benoit.clem@gmail.com'

	# Credentials (if needed)
	username = 'zagett.new.year@gmail.com'
	password = 'clem7,tiag'

	# The actual mail send
	server = smtplib.SMTP('smtp.gmail.com:587')
	server.starttls()
	server.login(username,password)
	server.sendmail(fromaddr, toaddrs, msg.as_string())
	server.quit()

def postMail(text):
	global q
	print("post Mail: " + text)
	q.put(text)

def threadMail():
	while True:
		try:
			text = q.get(timeout = 2)
			print("Send Mail: " + text)
			sendMail(text)
		except Empty:
			pass

class MyTCPHandler(SocketServer.BaseRequestHandler):
	dh = None

	def server_bind(self):
		self.socket.setsockopt(SOL_SOCKET, SO_REUSEADDR,SO_REUSEPORT, 1)
		self.socket.bind(self.server_address)
		self.socket.setblocking(0)

	
	def handle(self):
		global lastWeather
		# set the dh
		if self.dh == None:
			self.dh = dataHandler("data")
		# self.request is the TCP socket connected to the client
		addr = self.client_address[0]
		lKey = "cdbb976f33a36af55b892edd363a8b38f0191f254a8a68020429a5c2ecb85b63"
		wKey = "c0b843a96f930cfcb50eb2cfe65760c5"

		try:
			print ("======== connection from:" + str(addr) + " ======== ")
			jdata = self.request.recv(1024).strip()
			data = json.loads(jdata)
			ident = data["id"]
			request = data["request"]
			print ("- ident " + str(ident) + " requesting " + str(request))
		except ValueError:
			print ("- NO JSON ... QUIT")
			print ("======== connection from:" + str(addr) + " ======== ")
			return

		response = ""

		# this is commands emmited by admin
		if ident == "admin":
			print("- This is an admin request: " + request)
			if request == "listClients":
				response = json.dumps(self.dh.listClients())
			elif request == "client":
				print(data)
				client = data["client"]
				response = json.dumps(self.dh.getClient(client))
			elif request == "update":
				client = data["client"]
				
				try:
					message = data["message"]
				except ValueError:
					message = None
				
				try:
					quote = data["quote"]
				except ValueError:
					message = None
				
				try:
					brief = data["brief"]
				except ValueError:
					message = None
				
				self.dh.updateClient(client,message,quote,brief)
				response = json.dumps({})
		else:
			# Check if client exists
			if not self.dh.clientExists(ident):
				print("- Client Does not exists")
				if request == "name":
					name = data["name"]
					print("- Add new client " + str(name))
					self.dh.addClient(ident,name,quote="zagett is feeling proud to introduce you the zaza, the best way to keep in touch with our bests client!!!",brief = "Brief1:;Brief2:;Brief3:;Brief4:;")
					response = "OK\r"
				else:
					# default
					response = "OK\r"
			else:
				# Process request
				print("- Client exists")
				if request == "wheather":
					print("- Request location from ip: " + str(addr))
					#try:
					locResponse = urllib2.urlopen('http://api.ipinfodb.com/v3/ip-city/?key=%s&ip=%s&format=json' % (lKey,addr)).read()
					jlocResponse = json.loads(locResponse)
					if jlocResponse["statusCode"] == "OK":
						lat = float(jlocResponse["latitude"])
						lon = float(jlocResponse["longitude"])
						print("- Request weather for " + str(ident) + " location(" + str(lat) + ", " + str(lon) + ")")
						wheaResponse = urllib2.urlopen('http://api.openweathermap.org/data/2.5/weather?lat=%f&lon=%f&APPID=%s' % (lat,lon,wKey)).read()
						jWheaResponse = json.loads(wheaResponse)
						try:
							temp = jWheaResponse["main"]["temp"]-273.15
							iconStr = jWheaResponse["weather"][0]["icon"]
							iconId = iconStr[0:2]
							if iconStr[2:3] == 'd':
								night = 0
							else:
								night = 1
							d = datetime.now()
							#print(iconId,temp,night)
							#print(d.day,d.month,int(str(d.year)[2:4]),d.hour,d.minute,int(iconId),night,int(temp))
							print("- Temperature is " + str(temp) + "degC icon is " + str(iconId) + " and night is " + str(night))
							response = pack("=BBBBBBBB",d.day,d.month,int(str(d.year)[2:4]),d.hour,d.minute,int(iconId),night,int(temp))
							response += "\r"
							lastWeather = response
						except KeyError as e:
							print("- !!! got key error");
							print(e)
							response = "OK\r"
					else:
						response = "OK\r"
					"""
					except e:
						print(e)
						print('Issue when fetching weather use last wheather')
						response = lastWeather
					"""
				elif request == "quote":
					maxSz = data["szMax"]
					spaceFont = data["spaceFont"]
					rawResponse  = self.dh.get("quote",ident)
					#rawResponse = "zagett is feeling proud to introduce you the zaza, the best way to keep in touch with our bests client!!!"
					response = splitSentence(4,maxSz,spaceFont,rawResponse)+'\r'
				elif request == "message":
					maxSz = data["szMax"]
					spaceFont = data["spaceFont"]
					rawResponse  = self.dh.get("message",ident)
					msgStatus = self.dh.get("messageStatus",ident)
					#rawResponse = "On irai pas manger ensemble a la brasserie boulevard saint germain le 22 juin a 10h30 pour parler du projet gegett?"
					response = str(msgStatus) + splitSentence(4,maxSz,spaceFont,rawResponse)+'\r'
				elif request == "briefs":
					response = self.dh.get("brief",ident)
					if response == None:
						response = "Brief1:;Brief2:;Brief3:;Brief4:;\r"
					else:
						response += "\r"
				elif request == "ackMsg":
					print("- gotAckMessage")
					self.dh.ackMessage(ident)
					response = "OK\r"
				elif request == "postBrief":
					idBrief = data["idBrief"]
					print("- gotBrief :" + str(idBrief))
					postMail("Nouveau Brief " + str(idBrief) + " de " + ident)
					response = "OK\r"
				else:
					# default
					response = "OK\r"
		print("- response: >>" + response )
		response = response.upper()
		self.request.sendall(response)
		print ("============= DONE ============== ")

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 3333

    # Run Thread
    mail = Thread(target=threadMail)
    mail.start()

    # Create the server, binding to localhost on port 9999
    server = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)

    # Activate the server; this will keep running until you
    # interrupt the program with Ctrl-C
    server.serve_forever()
