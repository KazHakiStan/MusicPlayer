param(
    [string]$InstallDir = "$env:LOCALAPPDATA\MusicPlayer"
)

$ErrorActionPreference = "Stop"

Write-Host "Installing MusicPlayer to $InstallDir ..."

# 1. Create install dir
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir | Out-Null
}

# 2. Get latest release info from GitHub
$repo = "KazHakiStan/MusicPlayer"   # e.g. denisdev/cli-music-player
$apiUrl = "https://api.github.com/repos/$repo/releases/latest"

Write-Host "Checking latest release from $apiUrl"
$release = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "MusicPlayer-Installer" }

$tag = $release.tag_name
Write-Host "Latest version is $tag"

# 3. Find Windows zip asset
# Convention: asset named like MusicPlayer-win64.zip
$asset = $release.assets | Where-Object { $_.name -like "*win64*.zip" } | Select-Object -First 1
if (-not $asset) {
    throw "No win64 zip asset found in latest release."
}

$tempZip = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), $asset.name)
Write-Host "Downloading $($asset.browser_download_url) ..."
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tempZip

# 4. Extract into install dir (overwrite existing files)
Write-Host "Extracting to $InstallDir ..."
Expand-Archive -Path $tempZip -DestinationPath $InstallDir -Force
Remove-Item $tempZip -Force

# 5. Write version file
"$tag" | Out-File -FilePath (Join-Path $InstallDir "VERSION.txt") -Encoding utf8

# 6. Add install dir to user PATH
$current = [Environment]::GetEnvironmentVariable("Path", "User")
if ($current -notlike "*$InstallDir*") {
    Write-Host "Adding $InstallDir to user PATH ..."
    $new = if ([string]::IsNullOrEmpty($current)) {
        $InstallDir
    } else {
        "$current;$InstallDir"
    }
    [Environment]::SetEnvironmentVariable("Path", $new, "User")
} else {
    Write-Host "$InstallDir is already on PATH."
}

Write-Host ""
Write-Host "Installation complete."
Write-Host "If this is your first install, open a NEW terminal and run: musicplayer"


