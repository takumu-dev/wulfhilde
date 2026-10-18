# Installs Vulkan on a Windows runner, which has no GPU and so no Vulkan driver: the Khronos loader
# and vulkaninfo from LunarG's runtime components, and lavapipe from mesa-dist-win's MSVC build,
# each pinned by version and SHA-256.
#
# Both are installed system-wide, as a GPU driver would install them. The loader replaces the one
# in the system directory, whose version is whatever the image's last update left. Lavapipe is
# registered under HKLM: the runner's processes are elevated, and the loader ignores its
# environment variables, VK_DRIVER_FILES among them, in an elevated process.
#
# Unlike the Linux image, this leaves lavapipe's CPU features as the host has them: no Windows job
# compares what it renders with the reference images.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
# The progress bar slows a download down several times over.
$ProgressPreference = 'SilentlyContinue'

$loader = @{
    Version = '1.4.363.0'
    Sha256  = 'a25a927aa8b9f0371048f1861cf88ac3b9bc9b1fb332c42d897c8ab32695769a'
}
$loader.Url = "https://sdk.lunarg.com/sdk/download/$($loader.Version)/windows/VulkanRT-X64-$($loader.Version)-Components.zip"

$mesa = @{
    Version = '26.2.4'
    Sha256  = '351fc8c8b695878ffb3eaa044b3ead08672a48b1a045e3c3e3975811df0f6695'
}
$mesa.Url = "https://github.com/pal1000/mesa-dist-win/releases/download/$($mesa.Version)/mesa3d-$($mesa.Version)-release-msvc.7z"

# Where lavapipe stays, since its registration names its manifest by path.
$lavapipeDir = Join-Path $env:ProgramFiles 'Mesa'

# Downloads a file into the given directory and checks it against its SHA-256.
function Get-PinnedFile([hashtable] $Pin, [string] $Directory) {
    $path = Join-Path $Directory ([IO.Path]::GetFileName($Pin.Url))
    Invoke-WebRequest -Uri $Pin.Url -OutFile $path -MaximumRetryCount 3 -RetryIntervalSec 5
    $actual = (Get-FileHash -Path $path -Algorithm SHA256).Hash
    if ($actual -ne $Pin.Sha256) {
        throw "install-vulkan: $($Pin.Url) has SHA-256 $actual, not $($Pin.Sha256)"
    }
    return $path
}

$work = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid()))

$archive = Get-PinnedFile $loader $work
Expand-Archive -Path $archive -DestinationPath $work
$components = Join-Path $work "VulkanRT-X64-$($loader.Version)-Components\x64"
$system = [Environment]::GetFolderPath('System')
Copy-Item -Path (Join-Path $components 'vulkan-1.dll'), (Join-Path $components 'vulkaninfo.exe') -Destination $system -Force

# The archive holds every Mesa driver; lavapipe is one library and its manifest.
$archive = Get-PinnedFile $mesa $work
7z x $archive "-o$lavapipeDir" x64\vulkan_lvp.dll x64\lvp_icd.x86_64.json -y | Out-Null
$manifest = Join-Path $lavapipeDir 'x64\lvp_icd.x86_64.json'
$drivers = 'HKLM:\SOFTWARE\Khronos\Vulkan\Drivers'
# New-Item -Force would empty a key that exists.
if (-not (Test-Path $drivers)) {
    New-Item -Path $drivers -Force | Out-Null
}
New-ItemProperty -Path $drivers -Name $manifest -PropertyType DWord -Value 0 -Force | Out-Null

Remove-Item -Recurse -Force $work
Write-Output "install-vulkan: the Vulkan loader $($loader.Version) and lavapipe from Mesa $($mesa.Version)"
