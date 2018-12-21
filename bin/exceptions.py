#!/usr/bin/env python3
# -*- code:utf-8 -*-
"""
Name: Video2X Exceptions
Dev: K4YT3X
Date Created: December 13, 2018
Last Modified: December 19, 2018
"""


class ArgumentError(Exception):
    def __init__(self, message):
        super().__init__(message)


class InvalidModelType(Exception):
    def __init__(self, message):
        super().__init__(message)
