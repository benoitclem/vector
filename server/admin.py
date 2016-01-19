#!/opt/local/bin/python2.7
# coding: utf8


import sys
import socket
import json
from PyQt4 import QtGui,QtCore

HOST = 'carbon14.arichard.fr'
PORT = 3333

def request(data):
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.connect((HOST, PORT))
	s.sendall(data)
	data = s.recv(1024)
	s.close()
	return json.loads(data)

class zazAdmin(QtGui.QWidget):

	def __init__(self):
		super(zazAdmin, self).__init__()
		self.initUI()
		self.initData()

	def itemClicked(self,v):
		clientId = str(v.text()[0:17])
		print(clientId)
		self.fillUpClient(clientId)

	def updateClicked(self,v):
		item = self.clientsList.currentItem()
		clientId = str(item.text()[0:17])
		quote = str(self.quoteEdit.toPlainText())
		message = str(self.messageEdit.toPlainText())
		self.updateClient(clientId,message,quote,None)

	def initUI(self):
		
		clients = QtGui.QLabel('Clients')
		self.clientsList = QtGui.QListWidget()
		self.clientsList.setMaximumSize(QtCore.QSize(200,1000))
		self.clientsList.itemClicked.connect(self.itemClicked)
		quote = QtGui.QLabel('Quote')
		message = QtGui.QLabel('Message')

		self.quoteEdit = QtGui.QTextEdit()
		self.messageEdit = QtGui.QTextEdit()

		updateBtn = QtGui.QPushButton('update', self)
		updateBtn.clicked.connect(self.updateClicked)
		hbox = QtGui.QHBoxLayout()

		vboxLeft = QtGui.QVBoxLayout()
		vboxRight = QtGui.QVBoxLayout()

		hbox.addLayout(vboxLeft)
		hbox.addLayout(vboxRight)

		vboxLeft.addWidget(clients)
		vboxLeft.addWidget(self.clientsList)

		#vboxRight.addStretch(1)
		vboxRight.addWidget(quote)
		vboxRight.addWidget(self.quoteEdit)
		vboxRight.addWidget(message)
		vboxRight.addWidget(self.messageEdit) 
		#vboxRight.addStretch(1)
		vboxRight.addWidget(updateBtn)      

		self.setLayout(hbox)

		self.resize(1000, 400)
		self.setWindowTitle('zazAdmin') 
		#self.center() 
		self.show()

	def initData(self):
		jdata = json.dumps({'id':'admin','request':'listClients'})
		clients = request(jdata)
		for client in clients:
			self.clientsList.addItem(str(client))

	def fillUpClient(self,client):
		jdata = json.dumps({'id':'admin','request':'client','client':str(client).lower()})
		dataClient = request(jdata)
		self.quoteEdit.setText(dataClient["QUOTE"].lower())
		self.messageEdit.setText(dataClient["MESSAGE"].lower())

	def updateClient(self,client,message,quote,brief):
		jdata = json.dumps({'id':'admin','request':'update','client':str(client).lower(),'message':message,'quote':quote,'brief':brief})
		yay = request(jdata)

def main():
	app = QtGui.QApplication(sys.argv)
	ex = zazAdmin()
	sys.exit(app.exec_())


if __name__ == '__main__':
	main()