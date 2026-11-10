# Configures, builds and tests one workflow preset in a developer command prompt for MSVC.
#
#   ci/windows/build.ps1 -MsvcVersion 14.52 -Preset msvc-debug
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^14\.\d{2}$')]
    [string] $MsvcVersion,

    [Parameter(Mandatory)]
    [string] $Preset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

& (Join-Path $PSScriptRoot 'enter-msvc.ps1') -Version $MsvcVersion
# Ahead of the CMake and Ninja that Visual Studio's own tools put on the PATH.
$env:PATH = "$(Join-Path $env:CI_TOOLS 'Scripts');$env:PATH"
cmake --workflow --preset $Preset
