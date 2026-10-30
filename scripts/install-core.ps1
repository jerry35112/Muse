param([string]$Version = 'v1.19.33')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$headers = @{ 'User-Agent' = 'Muse-development'; 'Accept' = 'application/vnd.github+json' }
if ($Version -eq 'v1.19.33') {
    # Pinned release metadata from the official GitHub asset page. Works without API quota.
    $release = [pscustomobject]@{ tag_name = $Version; assets = @([pscustomobject]@{
        name = 'mihomo-windows-amd64-compatible-v1.19.33.zip'
        browser_download_url = 'https://github.com/MetaCubeX/mihomo/releases/download/v1.19.33/mihomo-windows-amd64-compatible-v1.19.33.zip'
        digest = 'sha256:7aaf636a4f2172dcd8ec34ac4f52797b059195e5afd6409508c963e545f8864d'
    }) }
}
elseif ($Version -eq 'latest') { $release = Invoke-RestMethod 'https://api.github.com/repos/MetaCubeX/mihomo/releases/latest' -Headers $headers }
else {
    if ($Version -notmatch '^v\d+\.\d+\.\d+$') { throw 'Use latest or a vX.Y.Z release tag' }
    $release = Invoke-RestMethod "https://api.github.com/repos/MetaCubeX/mihomo/releases/tags/$Version" -Headers $headers
}
$asset = @($release.assets | Where-Object { $_.name -match '^mihomo-windows-amd64-compatible-.*\.zip$' })
if ($asset.Count -ne 1) { throw 'Expected exactly one Windows x64 compatible ZIP in the official release; install manually from https://github.com/MetaCubeX/mihomo/releases' }
$asset = $asset[0]
if ($asset.browser_download_url -notlike 'https://github.com/MetaCubeX/mihomo/releases/download/*') { throw 'Unexpected download origin' }
# GitHub exposes the digest of uploaded release assets. Do not run unverified downloads.
if ($asset.digest -notmatch '^sha256:([a-fA-F0-9]{64})$') { throw 'No SHA-256 digest available from GitHub. Download and verify the core manually.' }
$expected = $Matches[1]
$coreDir = Join-Path $projectRoot 'core'
New-Item -ItemType Directory -Force $coreDir | Out-Null
$archive = Join-Path $coreDir $asset.name
Invoke-WebRequest $asset.browser_download_url -OutFile $archive -Headers $headers
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Core checksum mismatch' }
# Unpack into a temporary directory: the archive contains a second copy of the
# ~60 MB binary, so it must not be left behind next to the installed core.
$extractDir = Join-Path ([System.IO.Path]::GetTempPath()) ('muse-core-' + [guid]::NewGuid().ToString('N'))
try {
    Expand-Archive -LiteralPath $archive -DestinationPath $extractDir
    $binary = @(Get-ChildItem -LiteralPath $extractDir -Filter '*.exe' -Recurse)
    if ($binary.Count -ne 1) { throw 'Unexpected core archive contents' }
    Copy-Item -LiteralPath $binary[0].FullName -Destination (Join-Path $coreDir 'mihomo.exe')
} finally {
    if (Test-Path -LiteralPath $extractDir) { Remove-Item -LiteralPath $extractDir -Recurse -Force }
}
Invoke-WebRequest "https://raw.githubusercontent.com/MetaCubeX/mihomo/$($release.tag_name)/LICENSE" -OutFile (Join-Path $coreDir 'mihomo-LICENSE.txt')
if (Test-Path "$projectRoot/dist") {
    New-Item -ItemType Directory -Force "$projectRoot/dist/core" | Out-Null
    Copy-Item -LiteralPath "$coreDir/mihomo.exe" -Destination "$projectRoot/dist/core/mihomo.exe"
}
Write-Host "Installed official mihomo $($release.tag_name); SHA-256 verified."
