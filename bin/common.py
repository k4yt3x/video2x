#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Common Code
Author: ddouglas87
Date Created: Sept 15, 2019
Last Modified: Sept 15, 2019

Description: This class contains helper functions.
"""

from pathlib import Path
import subprocess
import shutil


def find_path(path="", filename=""):
    """Looks for file in specified path. If not found, checks system path."""

    # Search in calling path
    if _find_file(path, filename):
        return _find_file(path, filename)

    # common.py's path
    current_path = Path(__file__).parent

    # Search in common.py's path (usually bin folder)
    if _find_file(current_path / path, filename):
        return _find_file(current_path / path, filename)

    # Search one directory back (usually video2x folder)
    if _find_file(current_path / '..' / path, filename):
        return _find_file(current_path / '..' / path, filename)

    # Find binary in $PATH
    if shutil.which(filename):
        return [Path(""), Path(shutil.which(filename)).name]

    # Find file in $PATH
    if filename and subprocess.run('command -v ' + filename, shell=True, stdout=subprocess.DEVNULL).returncode == 0:
        return [Path(""), filename]

    # Couldn't find file
    return [None, None]


def _find_file(path="", filename=""):
    """Looks both for file and for Windows users file.exe"""

    if (Path(path) / Path(filename + '.exe')).exists():
        return [Path(path), filename + '.exe']
    if (Path(path) / filename).exists():
        return [Path(path), filename]
