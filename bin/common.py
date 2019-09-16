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


def find_path(path, filename=""):
    """Looks for file in specified path. If not found, checks system path. Returns path if file is found."""
    if filename == "":
        filename = path

    if os.path.exists(Path.joinpath(Path('..'), path, filename + '.exe')):
        return Path.joinpath(Path('..'), path, filename + '.exe')
    elif os.path.exists(Path.joinpath(Path('..'), path, filename + '.jar')):
        return Path.joinpath(Path('..'), path, filename + '.jar')
    elif os.path.exists(Path.joinpath(Path('..'), path, filename)):
        return Path.joinpath(Path('..'), path, filename)
    elif os.path.exists(Path.joinpath(Path(path), filename + '.exe')):
        return Path.joinpath(Path(path), filename + '.exe')
    elif os.path.exists(Path.joinpath(Path(path), filename + '.jar')):
        return Path.joinpath(Path(path), filename + '.jar')
    elif os.path.exists(Path.joinpath(Path(path), filename)):
        return Path.joinpath(Path(path), filename)
    elif subprocess.run('command -v ' + filename + '.exe', shell=True).returncode == 0:
        return subprocess.check_output('command -v ' + filename + '.exe', shell=True).strip().decode('utf-8')
    elif subprocess.run('command -v ' + filename + '.exe', shell=True).returncode == 0:
        return subprocess.check_output('command -v ' + filename + '.exe', shell=True).strip().decode('utf-8')
    elif subprocess.run('command -v ' + filename, shell=True).returncode == 0:
        return subprocess.check_output('command -v ' + filename, shell=True).strip().decode('utf-8')
    else:
        return None
