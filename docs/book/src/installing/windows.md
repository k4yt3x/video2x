# Windows

You can download the latest version of the Windows build from the [releases page](https://github.com/k4yt3x/video2x/releases/latest). Here are the steps to download and install the pre-built binaries to `%LOCALAPPDATA%\Programs`.

```bash
$latestTag = (Invoke-RestMethod -Uri https://api.github.com/repos/k4yt3x/video2x/releases/latest).tag_name
curl -LO "https://github.com/k4yt3x/video2x/releases/download/$latestTag/video2x-windows-amd64.zip"
New-Item -Path "$env:LOCALAPPDATA\Programs\video2x" -ItemType Directory -Force
Expand-Archive -Path .\video2x-windows-amd64.zip -DestinationPath "$env:LOCALAPPDATA\Programs\video2x"
```

You can then add `%LOCALAPPDATA%\Programs\video2x` to your `PATH` environment variable to run `video2x` from the command line.
