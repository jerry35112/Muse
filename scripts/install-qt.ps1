$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$baseUrl='https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_683/qt6_683/'
$downloadDir=Join-Path $projectRoot '.tools/qt-download'
$qtDir=Join-Path $projectRoot '.tools/Qt'
New-Item -ItemType Directory -Force $downloadDir,$qtDir | Out-Null
[xml]$metadata=(Invoke-WebRequest ($baseUrl+'Updates.xml')).Content
$packageNames=@('qt.qt6.683.win64_msvc2022_64','qt.qt6.683.addons.qtshadertools.win64_msvc2022_64')
foreach($packageName in $packageNames){
    $package=$metadata.Updates.PackageUpdate | Where-Object {$_.Name -eq $packageName}
    if(!$package){throw "Qt package not found: $packageName"}
    foreach($name in ($package.DownloadableArchives -split ',\s*')){
        if($name -notmatch '^(qtbase|qtdeclarative|qtsvg|qttools|qtshadertools)-'){continue}
        $url=$baseUrl+$packageName+'/'+$package.Version+$name
        $path=Join-Path $downloadDir $name
        $checksumResponse=(Invoke-WebRequest ($url+'.sha1')).Content
        if($checksumResponse -is [byte[]]){$checksumResponse=[System.Text.Encoding]::UTF8.GetString($checksumResponse)}
        $digest=($checksumResponse.Trim() -split '\s+')[0]
        if($digest -notmatch '^[a-fA-F0-9]{40}$'){throw 'Invalid Qt checksum'}
        if(!(Test-Path $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA1).Hash -ne $digest){
            Write-Host "Downloading $name"
            Invoke-WebRequest $url -OutFile $path
        }
        if((Get-FileHash -LiteralPath $path -Algorithm SHA1).Hash -ne $digest){throw 'Qt checksum mismatch'}
        tar -xf $path -C $qtDir
        if($LASTEXITCODE -ne 0){throw 'Qt extraction failed'}
    }
}
# Archive builds use a build-machine prefix; qt.conf makes Qt tools relocatable.
$prefix=$qtDir
"[Paths]`nPrefix=.." | Set-Content -LiteralPath "$prefix/bin/qt.conf"
New-Item -ItemType Directory -Force "$projectRoot/core/licenses" | Out-Null
Invoke-WebRequest 'https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES/LGPL-3.0-only.txt' -OutFile "$projectRoot/core/licenses/Qt-LGPL-3.0.txt"
Write-Host "Qt SDK ready: $prefix"
