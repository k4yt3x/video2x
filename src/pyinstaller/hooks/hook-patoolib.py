# -----------------------------------------------------------------------------
# Copyright (c) 2013-2017, PyInstaller Development Team.
#
# Distributed under the terms of the GNU General Public License with exception
# for distributing bootloader.
#
# The full license is in the file COPYING.txt, distributed with this software.
# -----------------------------------------------------------------------------

"""
Name: PyInstaller patoolib Hook
Original solution: https://github.com/pyinstaller/pyinstaller/issues/3013

PyInstaller cannot find libraries imported by patoolib,
    since it uses importlib to import these modules.
"""

hiddenimports = [
    "patoolib.programs",
    "patoolib.programs.ar",
    "patoolib.programs.arc",
    "patoolib.programs.archmage",
    "patoolib.programs.bsdcpio",
    "patoolib.programs.bsdtar",
    "patoolib.programs.bzip2",
    "patoolib.programs.cabextract",
    "patoolib.programs.chmlib",
    "patoolib.programs.clzip",
    "patoolib.programs.compress",
    "patoolib.programs.cpio",
    "patoolib.programs.dpkg",
    "patoolib.programs.flac",
    "patoolib.programs.genisoimage",
    "patoolib.programs.gzip",
    "patoolib.programs.isoinfo",
    "patoolib.programs.lbzip2",
    "patoolib.programs.lcab",
    "patoolib.programs.lha",
    "patoolib.programs.lhasa",
    "patoolib.programs.lrzip",
    "patoolib.programs.lzip",
    "patoolib.programs.lzma",
    "patoolib.programs.lzop",
    "patoolib.programs.mac",
    "patoolib.programs.nomarch",
    "patoolib.programs.p7azip",
    "patoolib.programs.p7rzip",
    "patoolib.programs.p7zip",
    "patoolib.programs.pbzip2",
    "patoolib.programs.pdlzip",
    "patoolib.programs.pigz",
    "patoolib.programs.plzip",
    "patoolib.programs.py_bz2",
    "patoolib.programs.py_echo",
    "patoolib.programs.py_gzip",
    "patoolib.programs.py_lzma",
    "patoolib.programs.py_tarfile",
    "patoolib.programs.py_zipfile",
    "patoolib.programs.rar",
    "patoolib.programs.rpm",
    "patoolib.programs.rpm2cpio",
    "patoolib.programs.rzip",
    "patoolib.programs.shar",
    "patoolib.programs.shorten",
    "patoolib.programs.star",
    "patoolib.programs.tar",
    "patoolib.programs.unace",
    "patoolib.programs.unadf",
    "patoolib.programs.unalz",
    "patoolib.programs.uncompress",
    "patoolib.programs.unrar",
    "patoolib.programs.unshar",
    "patoolib.programs.unzip",
    "patoolib.programs.xdms",
    "patoolib.programs.xz",
    "patoolib.programs.zip",
    "patoolib.programs.zoo",
    "patoolib.programs.zopfli",
    "patoolib.programs.zpaq",
]
