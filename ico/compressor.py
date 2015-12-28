#! /usr/bin/python

import Image
from struct import *
import os

print("compressor")

class bitComp:
	
	def who():
		return "bit"
	
	def compress(self,data,sx,sy):
		if (sx % 8) != 0:
			print("Could not compress, X size must be a multiple of 8")
			return
		
		byteArray = []
		for l in range(sy):
			n = 0
			b = 0
			for c in range(sx):
				b = (b<<1) + (data[c,l]&0x01)
				n+=1
				if n == 8:
					byteArray.append(b)
					b = 0
					n = 0
		return byteArray
		
class altComp:

	def who():
		return "alternate"

	def compress(self,data,sx,sy,draw = False):
		if draw:
			for l in range(sx):
				for c in range(sy):
					if c == 31:
						print data[c,l]
					else:
						print data[c,l],
		
		countPixelArray = []
		currPixel = data[0,0] 
		countPixel = 0
		for l in range(sx):
			for c in range(sy):
				if currPixel == data[c,l]:
					countPixel += 1
				else:
					countPixelArray.append(countPixel)
					countPixel = 1
					currPixel = data[c,l]
		countPixelArray.append(countPixel)	
			
		return countPixelArray
		
	def decompress(self,sx,sy,data,reconstPixel):
		currPixel = 255
		i = 0
		for r in data:
			for k in range(r):
				l = i/sx
				c = i%sx
				reconstPixel[c,l] = currPixel
				i += 1
			if currPixel == 0:
				currPixel = 255
			else:
				currPixel = 0
		
		
# OPEN BMP FILE

#filePath = "wifi.bmp"
#filePath = "bubble.png"
#filePath = "letter.png"
#filePath = "phone.png"
#filePath = "pen.png"
#filePath = "wifi2.png"
#filePath = "setup.png"
#filePath = "wheather/sunny.png"

path = "weather"

l = os.listdir("weather")

for f in l:
	if f[-3:] == "png":
		im = Image.open(path+"/"+f)
		c1 = bitComp()

		pixels = im.load()
		sx = im.size[0]
		sy = im.size[1]
		mode = im.mode

		data = c1.compress(pixels,sx,sy)

		print("const char %sLogo[] PROGMEM= {" %f[0:-4])
		print("0x%02x, 0x%02x,"%(sx,sy))
		for i in range(len(data)/16):
			for j in range(16):
				print("0x%02x," %data[i*16+j]),
			print("")
		print("};\n\r")


"""
im = Image.open(filePath)
c = altComp()
c1 = bitComp()

pixels = im.load()
sx = im.size[0]
sy = im.size[1]
mode = im.mode
"""



"""
# ===============================

# COMPRESS
d = c.compress(pixels,sx,sy)

# PACK
packedData = ""

for counts in d:
	packedData += pack('=b',counts)

header = pack('=bbc',sx,sy,mode)
fullFrame = header + packedData
"""

# ===============================


"""
# COMPRESS
data = c1.compress(pixels,sx,sy)

print("0x%02x, 0x%02x,"%(sx,sy))
for i in range(len(data)/16):
	for j in range(16):
		print("0x%02x," %data[i*16+j]),
	print("")
"""



"""
# WRITE TO COMPRESSED FILE
f = open(filePath+".comp","w")
f.write(fullFrame)
f.close()

# READ FROM COMPRESSED FILE 
f = open(filePath+".comp","r")
fullFrame = f.read()
f.close

sx,sy,mode = unpack("=bbc",fullFrame[:3])

# UNPACK
unpackedData = []

for car in fullFrame[3:]:
	unpackedData.append(unpack('=b',car)[0]) 

# CREATE IMAGE TO SHOW
reconstImg = Image.new(mode, (sx,sy), None) # create a new black image
reconstPixel = reconstImg.load() # create the pixel map

c.decompress(sx,sy,unpackedData,reconstPixel)

reconstImg.show()
"""
