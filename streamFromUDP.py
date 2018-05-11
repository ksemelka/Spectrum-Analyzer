import threading
import select
import socket
import sys
import time

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Bind the socket to the port
server_address = ('localhost', 7124)
print('starting up on {} port {}'.format(*server_address))
sock.bind(server_address)
sock.settimeout(0.1)

while True:
	try:
		r, _, _ = select.select([sock], [], [], 0)
		if r:
			data = sock.recv(4096)
			print(data)
		else:
			print('\nwaiting for UDP packets')
			time.sleep(2)
	except (KeyboardInterrupt):
		print('\nexiting program')
		sys.exit()
		# raise
