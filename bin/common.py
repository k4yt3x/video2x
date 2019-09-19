#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Common Code
Author: ddouglas87
Date Created: Sept 15, 2019
Last Modified: Sept 15, 2019

Description: This class contains helper functions.
"""

import os
from pathlib import Path
import subprocess


def find_path(path="", filename=""):
    """Looks for file in specified path. If not found, checks system path."""

    # Converts abs path to relative path, ensuring searching one directory back works
    path = os.path.relpath(Path(path))

    # Search in specified path
    if os.path.exists(Path(path) / Path(filename + '.exe')):
        return [Path(path), filename + '.exe']
    elif os.path.exists(Path(path) / filename):
        return [Path(path), filename]

    # Search one directory back
    elif os.path.exists(Path('..') / path / Path(filename + '.exe')):
        return [Path('..') / path, filename + '.exe']
    elif os.path.exists(Path('..') / path / filename):
        return [Path('..') / path, filename]

    # Search for program in $PATH
    elif filename:
        if subprocess.run('command -v ' + filename + '.exe', shell=True, stdout=subprocess.DEVNULL).returncode == 0:
            return [Path(""), filename + '.exe']
        elif subprocess.run('command -v ' + filename, shell=True, stdout=subprocess.DEVNULL).returncode == 0:
            return [Path(""), filename]

    else:
        # Couldn't find file
        return [None, None]
