<#
Name: Video2X Build Script
Creator: K4YT3X
Date Created: May 6, 2020
Last Modified: May 13, 2020

Description: A PowerShell script that will build Video2X
executable (PE) releases automatically using PyInstaller.
This script is currently only tuned for K4YT3X's environment.

To start a PowerShell session with execution policy bypass
powershell -ExecutionPolicy Bypass
#>

if ($args.count -ne 1){
    Write-Host -ForegroundColor White "Usage:`n .\build.ps1 VIDEO2X_VERSION"
    Exit
}

# version number
$SCRIPT_VERSION = "1.0.1"
$VIDEO2X_VERSION = $args[0]

Write-Host -ForegroundColor White "Video2X Building Script Version $($SCRIPT_VERSION)
Starting to build Video2X release packages
Building Video2X release $($VIDEO2X_VERSION)"

# build Video2X CLI
Write-Host -ForegroundColor White "`nBuilding Video2X CLI"
pyinstaller --noconfirm --log-level=WARN `
    --onefile `
    --add-data="locale;locale" `
    --add-data="wrappers;wrappers" `
    --icon="images\video2x.ico" `
    video2x.py

# build Video2X GUI
Write-Host -ForegroundColor White "`nBuilding Video2X GUI"
pyinstaller --noconfirm --log-level=WARN `
    --onefile `
    --add-data="images;images" `
    --add-data="locale;locale" `
    --add-data="video2x_gui.ui;." `
    --add-data="wrappers;wrappers" `
    --icon="images\video2x.ico" `
    video2x_gui.py

# build setup script
Write-Host -ForegroundColor White "`nBuilding Video2X setup script"
pyinstaller --noconfirm --log-level=WARN `
    --onefile `
    --additional-hooks-dir "pyinstaller\hooks" `
    --add-data="locale;locale" `
    --add-data="pyinstaller\7z1900-extra;7z" `
    --icon="images\video2x.ico" `
    video2x_setup.py

# remove old builds if found
if (Test-Path "$($VIDEO2X_VERSION)" -PathType any) {
    Remove-Item -path "$($VIDEO2X_VERSION)" -recurse
}

# create build directory
New-Item "$($VIDEO2X_VERSION)" -ItemType Directory

# copy files into corresponding builds
# full edition
Write-Host -ForegroundColor White "`nCreating full package"
New-Item "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-full" -ItemType Directory
Copy-Item "dist\video2x.exe" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-full\"
Copy-Item "dist\video2x_gui.exe" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-full\"
Copy-Item -Path "$env:LOCALAPPDATA\video2x" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-full\dependencies" -Recurse

# overwrite paths to relative paths
(Get-Content "video2x.yaml").replace("%LOCALAPPDATA%\video2x", "dependencies") | Set-Content "video2x.yaml.relative"
Move-Item "video2x.yaml.relative" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-full\video2x.yaml"

# light edition
Write-Host -ForegroundColor White "`nCreating light package"
New-Item "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-light" -ItemType Directory
Copy-Item "dist\video2x.exe" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "dist\video2x_gui.exe" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "dist\video2x_setup.exe" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "video2x.yaml" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "requirements.txt" -Destination "$($VIDEO2X_VERSION)\video2x-$($VIDEO2X_VERSION)-win32-light\"

# clean up temporary files
Write-Host -ForegroundColor White "`nDeleting temporary files"
$pathsToRemove = "__pycache__", "build", "dist", "*.spec"

foreach ($path in $pathsToRemove){
    Write-Host "Removing path: $($path)"
    Remove-Item -path $path -recurse
}

Write-Host -ForegroundColor White "`nBuild script finished"
