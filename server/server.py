#!/usr/bin/python
# coding: utf8

import SocketServer
import urllib
import json

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

class MyTCPHandler(SocketServer.BaseRequestHandler):

    def handle(self):
		# self.request is the TCP socket connected to the client
		addr = self.client_address[0]
		lKey = "cdbb976f33a36af55b892edd363a8b38f0191f254a8a68020429a5c2ecb85b63"
		wKey = "c0b843a96f930cfcb50eb2cfe65760c5"
		print ("connection from:",addr)
		jdata = self.request.recv(1024).strip()
		print(jdata)
		data = json.loads(jdata)
		request = data["request"]
		ident = data["id"]
		response = ""
		print(ident + " try loading " + request)
		if request == "wheather":
			locResponse = urllib.urlopen('http://api.ipinfodb.com/v3/ip-city/?key=%s&ip=%s&format=json' % (lKey,addr)).read()
			jlocResponse = json.loads(locResponse)
			if jlocResponse["statusCode"] == "OK":
				lat = float(jlocResponse["latitude"])
				lon = float(jlocResponse["longitude"])
				print(lat,lon)
				wheaResponse = urllib.urlopen('http://api.openweathermap.org/data/2.5/weather?lat=%f&lon=%f&APPID=%s' % (lat,lon,wKey)).read()
				jWheaResponse = json.loads(wheaResponse)
				print(jWheaResponse)
				response = "OK\r"
			else:
				response = "OK\r"
		if request == "quote":
			maxSz = data["szMax"]
			spaceFont = data["spaceFont"]
			rawResponse = "zagett is feeling proud to introduce you the zaza, the best way to keep in touch with our bests client!!!"
			response = splitSentence(4,maxSz,spaceFont,rawResponse)+'\r'
		elif request == "message":
			maxSz = data["szMax"]
			spaceFont = data["spaceFont"]
			rawResponse = "On irai pas manger ensemble a la brasserie boulevard saint germain le 22 juin a 10h30 pour parler du projet gegett?"
			response = "2" + splitSentence(4,maxSz,spaceFont,rawResponse)+'\r'
		elif request == "briefs":
			response = "Brief1:;Brief2:;Brief3:;Brief4:;\r"
		elif request == "ackMsg":
			print("gotAckMessage")
			response = "OK\r"
		elif request == "postBrief":
			idBrief = data["idBrief"]
			print("gotBrief :" + str(idBrief))
			response = "OK\r"
		else:
			reponse = "OK\r"
		print("response: " + response)
		response = response.upper()
		self.request.sendall(response)

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 3333

    # Create the server, binding to localhost on port 9999
    server = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)

    # Activate the server; this will keep running until you
    # interrupt the program with Ctrl-C
    server.serve_forever()
