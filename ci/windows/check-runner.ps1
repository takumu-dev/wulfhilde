# Checks that a Windows runner keeps the promises CI relies on, once the job has set it up: the
# MSVC toolset the job names, building C++23 programs that run with and without AddressSanitizer;
# the CMake and Ninja the presets call; and lavapipe as the only Vulkan device. It reports every
# broken promise, not just the first.
param(
    # The MSVC toolset the job builds with, such as 14.52.
    [Parameter(Mandatory)]
    [ValidatePattern('^14\.\d{2}$')]
    [string] $MsvcVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$failures = 0

function Fail([string] $Message) {
    [Console]::Error.WriteLine("check-runner: $Message")
    $script:failures++
}

# Runs a program and returns the lines it printed on stdout and stderr, or reports why it couldn't
# run or failed and returns $null.
function Invoke-Program([string] $What, [string] $Program, [string[]] $Arguments) {
    if (-not (Get-Command $Program -ErrorAction SilentlyContinue)) {
        Fail "${What}: $Program isn't on the PATH"
        return $null
    }
    $lines = @(& $Program @Arguments 2>&1 | ForEach-Object { "$_" })
    if ($LASTEXITCODE -ne 0) {
        Fail "${What}: ``$Program $Arguments`` failed with ${LASTEXITCODE}: $($lines -join "`n")"
        return $null
    }
    return , $lines
}

# Runs a program and expects a line of its output to match a regular expression.
function Expect([string] $What, [string] $Pattern, [string] $Program, [string[]] $Arguments) {
    $lines = Invoke-Program $What $Program $Arguments
    if ($null -ne $lines -and ($lines -join "`n") -notmatch "(?m)$Pattern") {
        Fail "${What}: ``$Program $Arguments`` printed: $($lines -join "`n")"
    }
}

# Builds the program with cl and the given flags, and expects it to run and print 42. C++23 is
# /std:c++latest, as CMake asks MSVC for it.
function Expect-Program([string] $What, [string[]] $Flags) {
    $name = ($What -replace '[^A-Za-z0-9]', '_') + '.exe'
    $built = Invoke-Program $What 'cl' (@('/nologo', '/std:c++latest', '/EHsc', "/DEXPECTED_MSC_VER=$expectedMscVer") + $Flags + @('program.cpp', "/Fe:$name"))
    if ($null -ne $built) {
        Expect $What '^42$' (Join-Path $work $name) @()
    }
}

# cl's own version runs five majors ahead of the toolset's, and _MSC_VER is that version's major
# and minor run together: 1952 for MSVC 14.52.
$expectedMscVer = 1900 + [int] $MsvcVersion.Split('.')[1]

$work = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid()))
Push-Location $work

# The toolset, as the developer command prompt set it up, then as cl itself reports it while it
# builds C++23 with the standard library, with and without AddressSanitizer.
if ("$env:VCToolsVersion" -notlike "$MsvcVersion.*") {
    Fail "the environment isn't set up for MSVC ${MsvcVersion}: VCToolsVersion is '$env:VCToolsVersion'"
}
Set-Content -Path program.cpp -Value @'
#include <print>

static_assert(_MSC_VER == EXPECTED_MSC_VER, "cl is the MSVC toolset the job names");

int main() { std::println("{}", 42); }
'@
Expect-Program "MSVC $MsvcVersion" @()
Expect-Program "MSVC $MsvcVersion with AddressSanitizer" @('/fsanitize=address')

# The build tools the presets name. The image's CMake must come first, ahead of any older one.
$cmake = Invoke-Program 'CMake is 4.4 or newer' 'cmake' @('--version')
if ($null -ne $cmake) {
    if (($cmake -join "`n") -notmatch '^cmake version (\d+\.\d+)') {
        Fail "CMake is 4.4 or newer: ``cmake --version`` printed: $($cmake -join "`n")"
    } elseif ([version] $Matches[1] -lt [version] '4.4') {
        Fail "CMake is 4.4 or newer: the CMake on the PATH is $($Matches[1]), at $((Get-Command cmake).Source)"
    }
}
Expect 'Ninja is on the PATH' '^\d+\.\d+' 'ninja' @('--version')

# Lavapipe is the only device. The runner has no GPU, so anything else would be a driver the
# jobs didn't install.
$summary = Invoke-Program 'lavapipe is the Vulkan device' 'vulkaninfo' @('--summary')
if ($null -ne $summary) {
    $devices = @(
        foreach ($line in $summary) {
            if ($line -match '^\s*deviceName\s*=\s*(.*)$') {
                $Matches[1].Trim()
            }
        }
    )
    if ($devices.Count -ne 1) {
        Fail "lavapipe isn't the only Vulkan device: $($devices -join '; ')"
    } elseif ($devices[0] -notlike 'llvmpipe *') {
        Fail "the Vulkan device isn't lavapipe: $($devices[0])"
    }
}

Pop-Location
Remove-Item -Recurse -Force $work

if ($failures -ne 0) {
    [Console]::Error.WriteLine("check-runner: promises broken: $failures")
    exit 1
}
Write-Output 'check-runner: every promise holds'
