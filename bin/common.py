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
    current_path = Path(__file__).parent  # common.py's path

    # Search in specified path
    if os.path.exists(Path(path) / Path(filename + '.exe')):
        return [Path(path), filename + '.exe']
    elif os.path.exists(Path(path) / filename):
        return [Path(path), filename]
    # Search in common.py's path (usually bin folder)
    if os.path.exists(current_path / path / Path(filename + '.exe')):
        return [current_path / path, filename + '.exe']
    elif os.path.exists(current_path / path / filename):
        return [current_path / path, filename]
    # Search one directory back (usually video2x folder)
    elif os.path.exists(current_path / '..' / path / Path(filename + '.exe')):
        return [current_path / '..' / path, filename + '.exe']
    elif os.path.exists(current_path / '..' / path / filename):
        return [current_path / '..' / path, filename]
    # Search for program in $PATH
    elif filename:
        if subprocess.run('command -v ' + filename + '.exe', shell=True, stdout=subprocess.DEVNULL).returncode == 0:
            return [Path(""), filename + '.exe']
        elif subprocess.run('command -v ' + filename, shell=True, stdout=subprocess.DEVNULL).returncode == 0:
            return [Path(""), filename]

    # Couldn't find file
    return [None, None]
