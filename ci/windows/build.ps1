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

# The job is elevated, and the Vulkan loader ignores VK_ADD_LAYER_PATH in an elevated process, so
# the validation layers that this preset's vcpkg tree will hold are registered as a system's would
# be, in place of any that an earlier preset registered.
$layers = 'HKLM:\SOFTWARE\Khronos\Vulkan\ExplicitLayers'
if (-not (Test-Path $layers)) {
    New-Item -Path $layers -Force | Out-Null
}
$builds = Join-Path (Get-Location) 'build'
(Get-Item -Path $layers).Property |
    Where-Object { $_.StartsWith($builds, [StringComparison]::OrdinalIgnoreCase) } |
    ForEach-Object { Remove-ItemProperty -Path $layers -Name $_ }
$manifest = Join-Path $builds "$Preset\vcpkg_installed\x64-windows-release\bin\VkLayer_khronos_validation.json"
New-ItemProperty -Path $layers -Name $manifest -PropertyType DWord -Value 0 -Force | Out-Null

& (Join-Path $PSScriptRoot 'enter-msvc.ps1') -Version $MsvcVersion
# Ahead of the CMake and Ninja that Visual Studio's own tools put on the PATH.
$env:PATH = "$(Join-Path $env:CI_TOOLS 'Scripts');$env:PATH"
cmake --workflow --preset $Preset
