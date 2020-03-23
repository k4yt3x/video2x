#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Image Cleaner
Author: BrianPetkovsek
Date Created: March 24, 2019
Last Modified: July 27, 2019

Editor: K4YT3X
Last Modified: March 23, 2020

Editor: 28598519a
Last Modified: March 23, 2020

Description: This class is to remove the extracted frames
that have already been upscaled.
"""

# built-in imports
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
        output_frames = [f.name for f in self.output_directory.iterdir() if f.is_file()]

        # compare and remove frames downscaled images that finished being upscaled
        # within each thread's  extracted frames directory
        for thread_id in range(self.threads):
            dir_path = self.input_directory / str(thread_id)

            # for each file within all the directories
            for file in dir_path.iterdir():
                # if file also exists in the output directory, then the file
                # has already been processed, thus not needed anymore
                if file.is_file() and file.name in output_frames:
                    file.unlink()
                    output_frames.remove(file.name)
