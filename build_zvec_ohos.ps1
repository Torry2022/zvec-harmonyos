param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDir,

    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$ProtocPath,

    [string]$OpenHarmonySdk = $env:DEVECO_SDK_HOME,
    [string]$ExpectedCommit = "cfe9eed2f8c010d0d68add1a69cffb91fa0fbbb6"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($OpenHarmonySdk)) {
    throw "Pass -OpenHarmonySdk or set DEVECO_SDK_HOME to the DevEco OpenHarmony SDK directory"
}

$SourceDir = (Resolve-Path $SourceDir).Path
$ProtocPath = (Resolve-Path $ProtocPath).Path
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$Toolchain = Join-Path $OpenHarmonySdk "native\build\cmake\ohos.toolchain.cmake"
$Cmake = Join-Path $OpenHarmonySdk "native\build-tools\cmake\bin\cmake.exe"
$Ninja = Join-Path $OpenHarmonySdk "native\build-tools\cmake\bin\ninja.exe"
$Patch = Join-Path $PSScriptRoot "patches\zvec-0.4.0-ohos.patch"

foreach ($Path in @($Toolchain, $Cmake, $Ninja, $Patch)) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required file not found: $Path"
    }
}

$ActualCommit = (& git -C $SourceDir rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $ActualCommit -ne $ExpectedCommit) {
    throw "Expected Zvec commit $ExpectedCommit, found $ActualCommit"
}

function Invoke-GitApply {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$SuppressError
    )

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    if ($SuppressError) {
        & git -C $SourceDir apply @Arguments 2>$null
    } else {
        & git -C $SourceDir apply @Arguments
    }
    $ExitCode = $LASTEXITCODE
    $ErrorActionPreference = $PreviousErrorActionPreference
    return $ExitCode
}

$ReverseCheckExitCode = Invoke-GitApply -Arguments @("--reverse", "--check", $Patch) -SuppressError
if ($ReverseCheckExitCode -ne 0) {
    $CheckExitCode = Invoke-GitApply -Arguments @("--check", $Patch)
    if ($CheckExitCode -ne 0) {
        throw "The HarmonyOS patch cannot be applied cleanly"
    }
    $ApplyExitCode = Invoke-GitApply -Arguments @($Patch)
    if ($ApplyExitCode -ne 0) {
        throw "Applying the HarmonyOS patch failed"
    }
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$ConfigureArgs = @(
    "-S", $SourceDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$Ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DOHOS_ARCH=arm64-v8a",
    "-DOHOS_STL=c++_shared",
    "-DOHOS_PLATFORM_LEVEL=9",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_C_BINDINGS=ON",
    "-DBUILD_PYTHON_BINDINGS=OFF",
    "-DBUILD_TESTING=OFF",
    "-DBUILD_TOOLS=OFF",
    "-DENABLE_ARMV8A=ON",
    "-DGLOBAL_CC_PROTOBUF_PROTOC=$ProtocPath",
    "-DUSE_OSS_MIRROR=ON"
)

& $Cmake @ConfigureArgs
if ($LASTEXITCODE -ne 0) {
    throw "Configuring Zvec failed"
}

& $Cmake --build $BuildDir --target zvec_c_api --parallel 6
if ($LASTEXITCODE -ne 0) {
    throw "Building zvec_c_api failed"
}

$Output = Join-Path $BuildDir "lib\libzvec_c_api.so"
if (-not (Test-Path -LiteralPath $Output)) {
    throw "Build completed without expected output: $Output"
}

$Hash = Get-FileHash -LiteralPath $Output -Algorithm SHA256
Write-Host "Built $Output"
Write-Host "SHA256 $($Hash.Hash)"
