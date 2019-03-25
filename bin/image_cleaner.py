#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Image Cleaner
Author: BrianPetkovsek
Author: K4YT3X
Date Created: March 24, 2019
Last Modified: March 24, 2019

Description: This class is to remove the extracted frames
that have already been upscaled.
"""

import os
import threading
import time


class ImageCleaner(threading.Thread):
    """ Video2X Image Cleaner

    This class creates an object that keeps track of extracted
    frames that has already been upscaled and are not needed
    anymore. It then deletes them to save disk space.

    Extends:
        threading.Thread
    """

    def __init__(self, input_folder, output_folder, num_threads):
        threading.Thread.__init__(self)
        self.input_folder = input_folder
        self.output_folder = output_folder
        self.num_threads = num_threads
        self.running = False

    def run(self):
        """ Run image cleaner
        """
        self.running = True

        while self.running:
            self.remove_upscaled_frames()
            time.sleep(1)

    def stop(self):
        """ Stop the image cleaner
        """
        self.running = False
        self.join()

    def remove_upscaled_frames(self):
        """ remove frames that have already been upscaled

        This method compares the files in the extracted frames
        folder with the upscaled frames folder, and removes
        the frames that has already been upscaled.
        """

        # list all images in the extracted frames
        output_frames = [f for f in os.listdir(self.output_folder) if os.path.isfile(os.path.join(self.output_folder, f))]

        # compare and remove frames downscaled images that finished being upscaled
        # within each thread's  extracted frames folder
        for i in range(self.num_threads):
            dir_path = os.path.join(self.input_folder, str(i))

            # for each file within all the folders
            for f in os.listdir(dir_path):
                file_path = os.path.join(dir_path, f)

                # if file also exists in the output folder, then the file
                # has already been processed, thus not needed anymore
                if os.path.isfile(file_path) and f in output_frames:
                    os.remove(file_path)
                    output_frames.remove(f)
