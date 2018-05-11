import pyqtgraph as pg
import numpy as np
from pyqtgraph.Qt import QtGui, QtCore
import pyqtgraph as pg
from scipy.fftpack import fft
import time
import sys
import struct
import select
import socket

class test(object):
	def __init__(self):
		# Create a UDP socket
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		# Bind the socket to the port
		self.server_address = ('localhost', 7124)
		print('starting up on {} port {}'.format(*self.server_address))
		self.sock.bind(self.server_address)
		
		# How many samples per second
		self.RATE = 384000
		# How many bytes are read for each frame
		self.CHUNK = int(4)
		self.scale = 4
		# self.stream = open("sinewave440Hz.dat", "rb")
		pg.setConfigOptions(antialias=True, useOpenGL=True)
		self.traces = dict()
		self.app = QtGui.QApplication(sys.argv)
		self.win = pg.GraphicsWindow(title='Spectrum Analyzer')
		self.win.setWindowTitle('Spectrum Analyzer')
		self.win.setGeometry(160, 90, 1600, 900)
		# wf_xlabels = [(0, '0'), (2048, '2048'), (4096, '4096'), (8192, '8192'), (16384, '16384')]
		# wf_xaxis = pg.AxisItem(orientation='bottom')
		# wf_xaxis.setTicks([wf_xlabels])

		# wf_ylabels = [(0, '0'), (127, '128'), (255, '255')]
		# wf_yaxis = pg.AxisItem(orientation='left')
		# wf_yaxis.setTicks([wf_ylabels])

		# self.waveform = self.win.addPlot(
		#    title='WAVEFORM', row=1, col=1, axisItems={'bottom': wf_xaxis, 'left': wf_yaxis},
		# )

		sp_xlabels = [
			(np.log10(1), '1'),
		   (np.log10(10), '10'), (np.log10(100), '100'),
		   (np.log10(1000), '1000'), (np.log10(10000), '10000'),
		   (np.log10(100000), '100000'), (np.log10(1000000), '1000000'),
		   (np.log10(10000000), '10000000')
		]
		sp_xaxis = pg.AxisItem(orientation='bottom')
		sp_xaxis.setTicks([sp_xlabels])

		self.spectrum = self.win.addPlot(
			title='SPECTRUM', row=1, col=1, axisItems={'bottom': sp_xaxis},
		)

		self.x = np.arange(0, 2 * self.CHUNK / self.scale, 2)
		self.f = np.linspace(0, self.RATE / 2, self.CHUNK / self.scale)

	def start(self):
		if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
			QtGui.QApplication.instance().exec_()

	def set_plotdata(self, name, data_x, data_y):
		if name in self.traces:
		   self.traces[name].setData(data_x, data_y)
		else:
			if name == 'waveform':
				self.traces[name] = self.waveform.plot(pen='c', width=3)
				self.waveform.setYRange(0, 1000000, padding=0)
				self.waveform.setXRange(0, 2 * self.CHUNK, padding=0.005)
			if name == 'spectrum':
				self.traces[name] = self.spectrum.plot(pen='m', width=3)
				self.spectrum.setLogMode(x=True, y=True)
				self.spectrum.setYRange(-4, 0, padding=0)
				self.spectrum.setXRange(
					np.log10(20), np.log10(self.RATE / 2), padding=0.005)

	def update(self):
		try:
			r, _, _ = select.select([self.sock], [], [], 0)
			if r:
				data = self.sock.recv(4096)
				data = struct.unpack('<' + str(sys.getsizeof(data)/4) + 'f', data)
				# data = np.array(wf_data, dtype='complex')[::self.scale]
				self.set_plotdata(name='spectrum', data_x=self.x, data_y=data,)
			else:
				print('\nwaiting for UDP packets')
				time.sleep(2)
		except (KeyboardInterrupt):
			print('\nexiting program')
			sys.exit()

		

		# wf: waveform
		# read a chunk of data (How many big should the chunk be?)
		# wf_data = self.stream.read(self.CHUNK)
		# print(wf_data)
		# wf_data = struct.unpack('<' + str(int(self.CHUNK / 4)) + 'f', wf_data)

		# Only read every 10th value into array because there's too much data to display anyways
		# 128 offset
		# wf_data = np.array(wf_data, dtype='complex')[::self.scale]

		# sp: spectrum
		# Take fft. Convert to int so can get rid of 128 offset from before
		# sp_data = fft(wf_data)
		# sp_data = np.abs(sp_data[0:int(self.CHUNK / 2)]
		#  	) * 2 / (128 * self.CHUNK)
		# sp_data = np.abs(sp_data)
		# print(sp_data)
		# self.set_plotdata(name='waveform', data_x=self.x, data_y=wf_data,)
		# self.set_plotdata(name='spectrum', data_x=self.f, data_y=sp_data,)		

	def animate(self):
		timer = QtCore.QTimer()
		timer.timeout.connect(self.update)
		timer.start()
		self.start()

if __name__ == '__main__':
	app = test()
	app.animate()
