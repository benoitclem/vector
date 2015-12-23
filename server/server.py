#!/usr/bin/python

import SocketServer
import json

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
			response = "zagett is feeling proud to introduce you the zaza\r"
		elif request == "briefs":
			response = "Brief1;Brief2;Brief3;Brief4;\r"
		else:
			reponse = "\r"
		self.request.sendall(response)

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 3333

    # Create the server, binding to localhost on port 9999
    server = SocketServer.TCPServer((HOST, PORT), MyTCPHandler)

    # Activate the server; this will keep running until you
    # interrupt the program with Ctrl-C
    server.serve_forever()
