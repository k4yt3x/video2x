#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (C) 2018-2022 K4YT3X and contributors.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

Name: Package Init
Author: K4YT3X
Date Created: July 3, 2021
Last Modified: February 11, 2022
"""

# version assignment has to precede imports to
# prevent setup.cfg from producing import errors
__version__ = "5.0.0-beta3"

# local imports
from .video2x import Video2X
from .upscaler import Upscaler
from .interpolator import Interpolator
