param(
    [switch]$Silent
)

$ErrorActionPreference = "Stop"

# Default install location
$InstallDir = "$env:LOCALAPPDATA\MusicPlayer"

if (-not (Test-Path $InstallDir)) {
    Write-Host "MusicPlayer is not installed at $InstallDir."
    exit 0
}

if (-not $Silent) {
    Write-Host "This will:"
    Write-Host "  - Stop any running musicplayer.exe"
    Write-Host "  - Remove $InstallDir"
    Write-Host "  - Remove $InstallDir from your user PATH"
    $answer = Read-Host "Continue? (y/N)"
    if ($answer -notin @('y','Y','yes','YES')) {
        Write-Host "Uninstall cancelled."
        exit 0
    }
}

# 1. Stop running processes (best-effort)
Get-Process musicplayer -ErrorAction SilentlyContinue | `
    Stop-Process -Force -ErrorAction SilentlyContinue

# 2. Remove from user PATH
$current = [Environment]::GetEnvironmentVariable("Path", "User")

if ($current) {
    $parts = $current.Split(';') | Where-Object {
        $_ -and ($_ -ne $InstallDir)
    }
    $newPath = ($parts -join ';')
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "Removed $InstallDir from user PATH (if it was present)."
} else {
    Write-Host "User PATH is empty or unavailable, skipping PATH cleanup."
}

# 3. Delete install directory
if (Test-Path $InstallDir) {
    Write-Host "Removing $InstallDir ..."
    Remove-Item -Recurse -Force $InstallDir
}

Write-Host "MusicPlayer has been uninstalled."
Write-Host "Open a NEW terminal for PATH changes to take effect."

