# increment-version.ps1
# Reads version.props (major.minor.patch) + build_counter.txt, increments counter,
# rewrites Package.appxmanifest and version.txt with full version.
# Called as PreBuildEvent by MSBuild.

$ErrorActionPreference = 'Stop'
$solutionDir = Split-Path -Parent $PSScriptRoot

$versionPropsPath = Join-Path $solutionDir 'version.props'
$counterPath     = Join-Path $solutionDir 'build_counter.txt'
$manifestPath    = Join-Path $solutionDir 'dosbox-uwp' 'Package.appxmanifest'
$versionTxtPath  = Join-Path $solutionDir 'version.txt'

# Read version.props — extract major, minor, patch from XML
[xml]$props = Get-Content $versionPropsPath
$ns = @{ m = 'http://schemas.microsoft.com/developer/msbuild/2003' }
$major = $props.Project.PropertyGroup.VersionMajor
$minor = $props.Project.PropertyGroup.VersionMinor
$patch = $props.Project.PropertyGroup.VersionPatch

# Read and increment counter
$buildNum = 0
if (Test-Path $counterPath) {
    $buildNum = [int](Get-Content $counterPath -Raw)
}
$buildNum++

# Write counter back
Set-Content -Path $counterPath -Value $buildNum -NoNewline

# Full version string
$fullVersion = "$major.$minor.$patch.$buildNum"

# Rewrite Package.appxmanifest — replace Version="x.x.x.x" on Identity element only
$manifest = Get-Content $manifestPath -Raw
$manifest = $manifest -replace '(<Identity[^>]*Version=")(\d+\.\d+\.\d+\.\d+)(")', "`${1}$fullVersion`${3}"
Set-Content -Path $manifestPath -Value $manifest -NoNewline

# Rewrite version.txt
Set-Content -Path $versionTxtPath -Value $fullVersion -NoNewline

Write-Host "[version] $fullVersion (build $buildNum)"
