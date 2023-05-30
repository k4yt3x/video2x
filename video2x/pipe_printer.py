#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (C) 2018-2023 K4YT3X and contributors.

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

Name: PIPE Printer
Author: K4YT3X <i@k4yt3x.com>
"""

import os
import sys
import threading
import time
from typing import IO


class PipePrinter(threading.Thread):
    def __init__(self, stderr: IO[bytes]) -> None:
        threading.Thread.__init__(self)
        self.stderr = stderr
        self.running = False

        # set read mode to non-blocking
        os.set_blocking(self.stderr.fileno(), False)

    def _print_output(self) -> None:
        output = self.stderr.read()
        if output is not None and len(output) != 0:
            print(output.decode(), file=sys.stderr)

    def run(self) -> None:
        self.running = True

        # keep printing contents in the PIPE
        while self.running is True:
            time.sleep(0.5)

            try:
                self._print_output()

            # pipe closed
            except ValueError:
                break

        return super().run()

    def stop(self) -> None:
        self.running = False
