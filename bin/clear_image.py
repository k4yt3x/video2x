#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Image clearer
Author: BrianPetkovsek
Date Created: March 24, 2019
Last Modified: March 25, 2019

Description: This class is to remove the 
downscaled image files when upscale is finished
from waifu2x-caffe.
"""

from threading import Thread
from time import sleep
import os

class ClearImage(Thread):
	def __init__(self, input_folder, output_folder,num_threads):
		Thread.__init__(self) 
		self.input_folder = input_folder
		self.output_folder = output_folder
		self.num_threads = num_threads
		self.running = False
 
	def run(self):
		self.running = True
		while(self.running):
			self.removeFrames()
			#delay in 1 second intrvals for stop trigger
			i=0
			while self.running and i<20:
				i+=1
				sleep(1)
				
		
	def stop(self):
		self.running = False
		self.join()
		
	def removeFrames(self):
		# list all images in the extracted frames
		output_frames = [f for f in os.listdir(self.output_folder) if os.path.isfile(os.path.join(self.output_folder, f))]
		
		# compare and remove frames downscaled images that finished being upscaled
		for i in range(self.num_threads):
			dir_path = os.path.join(self.input_folder,str(i))
			for f in os.listdir(dir_path):
				file_path = os.path.join(dir_path, f)
				if os.path.isfile(file_path) and f in output_frames:
					os.remove(file_path)
					output_frames.remove(f)
					
		
