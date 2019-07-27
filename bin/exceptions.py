#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Exceptions
Dev: K4YT3X
Date Created: December 13, 2018
Last Modified: July 27, 2019
"""


class ArgumentError(Exception):
    def __init__(self, message):
        super().__init__(message)


class StreamNotFoundError(Exception):
    def __init__(self, message):
        super().__init__(message)


class UnrecognizedDriverError(Exception):
    def __init__(self, message):
        super().__init__(message)


class UnsupportedPixelError(Exception):
    def __init__(self, message):
        super().__init__(message)
