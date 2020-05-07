<#
Name: Video2X Build Script
Creator: K4YT3X
Date Created: May 6, 2020
Last Modified: May 6, 2020

Description: A PowerShell script that will build Video2X
executable (PE) releases automatically using PyInstaller.
This script is currently only tuned for K4YT3X's environment.

To start a PowerShell session with execution policy bypass
powershell –ExecutionPolicy Bypass
#>

# version number
$SCRIPT_VERSION = "1.0.0"
$VIDEO2X_VERSION = "4.0.0"

Write-Host -ForegroundColor White "Video2X Building Script Version $($SCRIPT_VERSION)
Starting to build Video2X release packages"

# build Video2X CLI
Write-Host -ForegroundColor White "`nBuilding Video2X CLI"
pyinstaller --noconfirm --log-level=WARN `
    --onefile `
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
    --icon="images\video2x.ico" `
    video2x_setup.py

# remove old builds if found
if (Test-Path "video2x-builds" -PathType any) {
    Remove-Item -path "video2x-builds" -recurse
}

# create build directory
New-Item "video2x-builds" -ItemType Directory

# copy files into corresponding builds
# full edition
Write-Host -ForegroundColor White "`nCreating full package"
New-Item "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-full" -ItemType Directory
Copy-Item "dist\video2x.exe" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-full\"
Copy-Item "dist\video2x_gui.exe" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-full\"
Copy-Item -Path "$env:LOCALAPPDATA\video2x" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-full\dependencies" -Recurse

# overwrite paths to relative paths
(Get-Content "video2x.yaml").replace("C:\Users\K4YT3X\AppData\Local\video2x\", "dependencies\") | Set-Content "video2x.yaml.relative"
Move-Item "video2x.yaml.relative" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-full\video2x.yaml"

# light edition
Write-Host -ForegroundColor White "`nCreating light package"
New-Item "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-light" -ItemType Directory
Copy-Item "dist\video2x.exe" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "dist\video2x_gui.exe" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "dist\video2x_setup.exe" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "video2x.yaml" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-light\"
Copy-Item "requirements.txt" -Destination "video2x-builds\video2x-$($VIDEO2X_VERSION)-win32-light\"

# clean up temporary files
Write-Host -ForegroundColor White "`nDeleting temporary files"
$pathsToRemove = "__pycache__", "build", "dist", "*.spec"

foreach ($path in $pathsToRemove){
    Write-Host "Removing path: $($path)"
    Remove-Item -path $path -recurse
}

Write-Host -ForegroundColor White "`nBuild script finished"
