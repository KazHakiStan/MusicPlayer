param(
    [switch]$CheckOnly
)

$ErrorActionPreference = "Stop"

$InstallDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath    = Join-Path $InstallDir "musicplayer.exe"
$versionFile = Join-Path $InstallDir "VERSION.txt"

# current version
$currentVersion = $null
if (Test-Path $versionFile) {
    $currentVersion = (Get-Content $versionFile | Select-Object -First 1).Trim()
} else {
    # fallback: ask exe
    if (Test-Path $exePath) {
        $currentVersion = & $exePath --version
    }
}

if (-not $currentVersion) {
    $currentVersion = "0.0.0"
}

Write-Host "Current version: $currentVersion"

# get latest from GitHub
$repo = "KazHakiStan/MusicPlayer"
$apiUrl = "https://api.github.com/repos/$repo/releases/latest"
$release = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "MusicPlayer-Updater" }
$latest = $release.tag_name

# Write-Host "Latest version:  $latest"
#
# if ($latest -eq $currentVersion) {
#     Write-Host "MusicPlayer is up to date."
#     if ($CheckOnly) { exit 0 }
#     return
# }

if ($CheckOnly) {
    # If up to date -> print nothing
    if ($latest -eq $currentVersion) {
        # no output
        exit 0
    } else {
        # ONLY print latest version, nothing else
        Write-Output $latest
        exit 0
    }
}

Write-Host "Update available! Updating to $latest ..."

# same asset logic as install.ps1
$asset = $release.assets | Where-Object { $_.name -like "*win64*.zip" } | Select-Object -First 1
if (-not $asset) {
    throw "No win64 zip asset found in latest release."
}

$tempZip = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), $asset.name)
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tempZip

# Kill running musicplayer.exe (optional / careful!)
Get-Process musicplayer -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

Expand-Archive -Path $tempZip -DestinationPath $InstallDir -Force
Remove-Item $tempZip -Force

"$latest" | Out-File -FilePath $versionFile -Encoding utf8

Write-Host "Update complete. You can now run: musicplayer"

