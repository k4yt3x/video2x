#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Creator: Video2X Bidirectional Logger
Author: K4YT3X
Date Created: June 4, 2020
Last Modified: June 4, 2020
"""

# built-in imports
import _io
import pathlib


class BiLogger(object):
    """ A bidirectional logger that both prints the output
    and log all output to file.

    Original code from: https://stackoverflow.com/a/14906787
    """

    def __init__(self, terminal: _io.TextIOWrapper, logfile: pathlib.Path):
        """ initialize BiLogger

        Args:
            terminal (_io.TextIOWrapper): original terminal IO wrapper
            logfile (pathlib.Path): target log file path object
        """
        self.terminal = terminal
        self.log = logfile.open(mode='a+')

    def write(self, message: str):
        """ write message to original terminal output and log file

        Args:
            message (str): message to write
        """
        self.terminal.write(message)
        self.terminal.flush()
        self.log.write(message)
        self.log.flush()

    def flush(self):
        """ flush logger (for compability only)
        """
        pass
