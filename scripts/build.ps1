param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtPrefix=Join-Path $projectRoot '.tools/Qt'
if (!(Test-Path "$qtPrefix/lib/cmake/Qt6/Qt6Config.cmake")) { throw 'Run scripts/install-qt.ps1 first, or set qtPrefix to an existing Qt 6.8+ SDK.' }
cmake -S $projectRoot -B "$projectRoot/build-qt" -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qtPrefix"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build "$projectRoot/build-qt" --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir "$projectRoot/build-qt" -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
$outputDir = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force "$outputDir/core" | Out-Null
New-Item -ItemType Directory -Force "$outputDir/licenses" | Out-Null
$jsonLicense = "$projectRoot/build-qt/_deps/json-src/LICENSE.MIT"
if (!(Test-Path $jsonLicense)) { $jsonLicense = "$projectRoot/build/_deps/json-src/LICENSE.MIT" }
Copy-Item -LiteralPath $jsonLicense -Destination "$outputDir/licenses/json-LICENSE.txt"
Copy-Item -LiteralPath "$projectRoot/README.md","$projectRoot/THIRD_PARTY_NOTICES.md","$projectRoot/LICENSE","$projectRoot/CHANGELOG.md" -Destination $outputDir
if (Test-Path "$projectRoot/core/mihomo-LICENSE.txt") { Copy-Item -LiteralPath "$projectRoot/core/mihomo-LICENSE.txt" -Destination "$outputDir/licenses/mihomo-LICENSE.txt" }
if (Test-Path "$projectRoot/core/licenses/Qt-LGPL-3.0.txt") { Copy-Item -LiteralPath "$projectRoot/core/licenses/Qt-LGPL-3.0.txt" -Destination "$outputDir/licenses/Qt-LGPL-3.0.txt" }
Copy-Item -LiteralPath "$projectRoot/build-qt/$Configuration/Muse.exe" -Destination "$outputDir/Muse.exe"
$deployMode=if($Configuration -eq 'Debug'){'--debug'}else{'--release'}
& "$qtPrefix/bin/windeployqt.exe" $deployMode --verbose 0 --qmldir "$projectRoot/qml" --no-translations --no-compiler-runtime --no-opengl-sw "$outputDir/Muse.exe"
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
if (Test-Path "$projectRoot/core/mihomo.exe") { Copy-Item -LiteralPath "$projectRoot/core/mihomo.exe" -Destination "$outputDir/core/mihomo.exe" }
if (Test-Path "$projectRoot/core/wintun.dll") { Copy-Item -LiteralPath "$projectRoot/core/wintun.dll" -Destination "$outputDir/core/wintun.dll" }
Write-Host "Built: $outputDir/Muse.exe"
