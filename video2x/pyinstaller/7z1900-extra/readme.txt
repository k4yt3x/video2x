7-Zip Extra 19.00
-----------------

7-Zip Extra is package of extra modules of 7-Zip. 

7-Zip Copyright (C) 1999-2019 Igor Pavlov.

7-Zip is free software. Read License.txt for more information about license.

Source code of binaries can be found at:
  http://www.7-zip.org/

This package contains the following files:

7za.exe     - standalone console version of 7-Zip with reduced formats support.
7za.dll     - library for working with 7z archives
7zxa.dll    - library for extracting from 7z archives
License.txt - license information
readme.txt  - this file

Far\        - plugin for Far Manager
x64\        - binaries for x64


All 32-bit binaries can work in:
  Windows 2000 / 2003 / 2008 / XP / Vista / 7 / 8 / 10
  and  in any Windows x64 version with WoW64 support.
All x64 binaries can work in any Windows x64 version. 

All binaries use msvcrt.dll.

7za.exe 
-------

7za.exe - is a standalone console version of 7-Zip with reduced formats support.

  Extra: 7za.exe             : support for only some formats of 7-Zip.
  7-Zip: 7z.exe with 7z.dll  : support for all formats of 7-Zip.

7za.exe and 7z.exe from 7-Zip have same command line interface.
7za.exe doesn't use external DLL files.

You can read Help File (7-zip.chm) from 7-Zip package for description 
of all commands and switches for 7za.exe and 7z.exe.

7za.exe features:

  - High compression ratio in 7z format
  - Supported formats:
      - Packing / unpacking: 7z, xz, ZIP, GZIP, BZIP2 and TAR 
      - Unpacking only: Z, lzma, CAB.
  - Highest compression ratio for ZIP and GZIP formats.
  - Fast compression and decompression
  - Strong AES-256 encryption in 7z and ZIP formats.

Note: LZMA SDK contains 7zr.exe - more reduced version of 7za.exe.
But you can use 7zr.exe as "public domain" code.



DLL files
---------

7za.dll and 7zxa.dll are reduced versions of 7z.dll from 7-Zip.
7za.dll and 7zxa.dll support only 7z format.
Note: 7z.dll is main DLL file that works with all archive types in 7-Zip.

7za.dll and 7zxa.dll support the following decoding methods:
    - LZMA, LZMA2, PPMD, BCJ, BCJ2, COPY, 7zAES, BZip2, Deflate.

7za.dll also supports 7z encoding with the following encoding methods:
    - LZMA, LZMA2, PPMD, BCJ, BCJ2, COPY, 7zAES.

7za.dll and 7zxa.dll work via COM interfaces.
But these DLLs don't use standard COM interfaces for objects creating.

Look also example code that calls DLL functions (in source code of 7-Zip):
 
 7zip\UI\Client7z

Another example of binary that uses these interface is 7-Zip itself.
The following binaries from 7-Zip use 7z.dll:
  - 7z.exe (console version)
  - 7zG.exe (GUI version)
  - 7zFM.exe (7-Zip File Manager)

Note: The source code of LZMA SDK also contains the code for similar DLLs
(DLLs without BZip2, Deflate support). And these files from LZMA SDK can be 
used as "public domain" code. If you use LZMA SDK files, you don't need to 
follow GNU LGPL rules, if you want to change the code.




License FAQ
-----------

Can I use the EXE or DLL files from 7-Zip in a commercial application?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Yes, but you are required to specify in documentation for your application:
  (1) that you used parts of the 7-Zip program, 
  (2) that 7-Zip is licensed under the GNU LGPL license and 
  (3) you must give a link to www.7-zip.org, where the source code can be found.


Can I use the source code of 7-Zip in a commercial application?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Since 7-Zip is licensed under the GNU LGPL you must follow the rules of that license. 
In brief, it means that any LGPL'ed code must remain licensed under the LGPL. 
For instance, you can change the code from 7-Zip or write a wrapper for some 
code from 7-Zip and compile it into a DLL; but, the source code of that DLL 
(including your modifications / additions / wrapper) must be licensed under 
the LGPL or GPL. 
Any other code in your application can be licensed as you wish. This scheme allows 
users and developers to change LGPL'ed code and recompile that DLL. That is the 
idea of free software. Read more here: http://www.gnu.org/. 



Note: You can look also LZMA SDK, which is available under a more liberal license.


---
End of document
