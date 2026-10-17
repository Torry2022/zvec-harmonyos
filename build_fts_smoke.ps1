param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$LibraryDir,
    [string]$OpenHarmonySdk=$env:DEVECO_SDK_HOME,
    [string]$BuildDir=(Join-Path $PSScriptRoot 'build/fts-smoke')
)
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($OpenHarmonySdk)) { throw 'Pass -OpenHarmonySdk or set DEVECO_SDK_HOME' }
$native=Join-Path (Resolve-Path $OpenHarmonySdk).Path 'native'
$source=(Resolve-Path $SourceDir).Path
$library=(Resolve-Path $LibraryDir).Path
$commit=(& git -C $source rev-parse HEAD).Trim()
if ($commit -ne '8321c1314a559fd5f909e92498f43e5194bf9b99') { throw 'Expected Zvec 0.7.0 source commit' }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$BuildDir=(Resolve-Path $BuildDir).Path
& "$native/llvm/bin/clang++.exe" --target=aarch64-linux-ohos "--sysroot=$native/sysroot" -std=c++17 -O2 `
    -I "$source/src/include" "$PSScriptRoot/smoke/fts_main.cpp" -L $library -lzvec_c_api '-Wl,-rpath,$ORIGIN' `
    -o "$BuildDir/zvec_fts_smoke"
if ($LASTEXITCODE -ne 0) { throw 'FTS smoke compilation failed' }
Copy-Item -LiteralPath "$library/libzvec_c_api.so","$native/llvm/lib/aarch64-linux-ohos/libc++_shared.so" -Destination $BuildDir
Get-FileHash -Algorithm SHA256 -LiteralPath "$BuildDir/libzvec_c_api.so","$BuildDir/zvec_fts_smoke"
