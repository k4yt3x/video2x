#!/usr/bin/env python3
# -*- coding: future_fstrings -*-


"""
Name: Video2X Image Cleaner
Author: BrianPetkovsek
Author: K4YT3X
Date Created: March 24, 2019
Last Modified: April 28, 2019

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

    def __init__(self, input_directory, output_directory, threads):
        threading.Thread.__init__(self)
        self.input_directory = input_directory
        self.output_directory = output_directory
        self.threads = threads
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
        directory with the upscaled frames directory, and removes
        the frames that has already been upscaled.
        """

        # list all images in the extracted frames
        output_frames = [f for f in os.listdir(self.output_directory) if os.path.isfile(os.path.join(self.output_directory, f))]

        # compare and remove frames downscaled images that finished being upscaled
        # within each thread's  extracted frames directory
        for i in range(self.threads):
            dir_path = os.path.join(self.input_directory, str(i))

            # for each file within all the directories
            for f in os.listdir(dir_path):
                file_path = os.path.join(dir_path, f)

                # if file also exists in the output directory, then the file
                # has already been processed, thus not needed anymore
                if os.path.isfile(file_path) and f in output_frames:
                    os.remove(file_path)
                    output_frames.remove(f)
