#! /usr/bin/python
import corr,socket,array
#import socket,pylab,matplotlib,math,corr,array
import struct
import time
import numpy as np


if __name__ == '__main__':
       	#fpga=corr.katcp_wrapper.FpgaClient('10.0.1.170')
       	IP = "10.10.10.2" #bind on IP addresses
       	#IP = "" #bind on all IP addresses
       	PORT = 12345
	counter = 0
	file_name = "fast-test.dat"
       	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
       	sock.bind((IP, PORT))
       	if PORT != -1:
               	print "10GbE port connect done!"
       	#data, addr = sock.recvfrom(4096+8)
	
      	#header = struct.unpack('<Q', data[0:8])[0]
#	for i in range(100):
	while(1):
#		with open(file_name,'a') as f:
       		data, addr = sock.recvfrom(4104)
#			np.save(f,data)
		if len(data) != 4104:
			counter += 1
			print "See !"
		print 'counter:', counter
		#print 'received',len(data),'bytes'
	#	print 'from', addr
		
