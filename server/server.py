#!/usr/bin/python

import SocketServer
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
		print ("connection from:",self.client_address[0])
		jdata = self.request.recv(1024).strip()
		print(jdata)
		data = json.loads(jdata)
		request = data["request"]
		ident = data["id"]
		response = ""
		print(ident + " try loading " + request)
		if request == "quote":
			maxSz = data["szMax"]
			spaceFont = data["spaceFont"]
			rawResponse = "zagett is feeling proud to introduce you the zaza, the best way to keep in touch with our bests client!!!"
			response = splitSentence(4,maxSz,spaceFont,rawResponse)+'\r'
		elif request == "briefs":
			response = "Brief1:;Brief2:;Brief3:;Brief4:;\r"
		elif request == "postBrief":
			idBrief = data["idBrief"]
			print("gotBrief :" + str(idBrief))
			response = "OK\r"
		else:
			reponse = "\r"
		print("response: " + response)
		self.request.sendall(response)

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 3333

    # Create the server, binding to localhost on port 9999
    server = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)

    # Activate the server; this will keep running until you
    # interrupt the program with Ctrl-C
    server.serve_forever()
