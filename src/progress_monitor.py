#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Upscale Progress Monitor
Author: K4YT3X
Date Created: May 7, 2020
Last Modified: September 9, 2020
"""

# built-in imports
import contextlib
import threading
import time

# third-party imports
from tqdm import tqdm


class ProgressMonitor(threading.Thread):
    """ progress monitor

    This class provides progress monitoring functionalities
    by keeping track of the amount of frames in the input
    directory and the output directory. This is originally
    suggested by @ArmandBernard.
    """

    def __init__(self, upscaler, extracted_frames_directories):
        threading.Thread.__init__(self)
        self.upscaler = upscaler
        self.extracted_frames_directories = extracted_frames_directories
        self.running = False

    def run(self):
        self.running = True

        with tqdm(total=self.upscaler.total_frames, ascii=True, desc=_('Processing: {} (pass {}/{})').format(self.upscaler.current_input_file.name, self.upscaler.current_pass, len(self.upscaler.scaling_jobs))) as progress_bar:
            # tqdm update method adds the value to the progress
            # bar instead of setting the value. Therefore, a delta
            # needs to be calculated.
            previous_cycle_frames = 0
            while self.running:

                with contextlib.suppress(FileNotFoundError):
                    upscaled_frames = [f for f in self.upscaler.upscaled_frames.iterdir() if str(f).lower().endswith(self.upscaler.extracted_frame_format.lower())]
                    if len(upscaled_frames) >= 1:
                        self.upscaler.last_frame_upscaled = sorted(upscaled_frames)[-1]
                    self.upscaler.total_frames_upscaled = len(upscaled_frames)

                    # update progress bar
                    delta = self.upscaler.total_frames_upscaled - previous_cycle_frames
                    previous_cycle_frames = self.upscaler.total_frames_upscaled
                    progress_bar.update(delta)

                time.sleep(1)

    def stop(self):
        self.running = False
        self.join()
