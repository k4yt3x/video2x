<#
Name: Video2X Generate POT Script
Creator: K4YT3X
Date Created: September 12, 2020
Last Modified: September 12, 2020

Description: A PowerShell script that uses Python's pygettext
script to generate the POT file for translations.

To start a PowerShell session with execution policy bypass
powershell -ExecutionPolicy Bypass
#>

python $env:LOCALAPPDATA\Programs\Python\Python38\Tools\i18n\pygettext.py -d video2x *.py wrappers\*.py
