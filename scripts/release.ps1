<#
.SYNOPSIS
    Release script: build with current version, then increment for next snapshot.
.DESCRIPTION
    Flow:
    1. Read version from version.txt (e.g. 0.0.0.1) — this is the RELEASE version
    2. Patch Package.appxmanifest with this version
    3. Build + package MSIX + create distributable zip
    4. Tag the release (if --Tag is set)
    5. Increment version.txt to next snapshot (e.g. 0.0.0.2)

    version.txt always holds the NEXT version to be released.
.PARAMETER Configuration
    Build configuration (default: Release)
.PARAMETER SkipBuild
    Skip build step (just package with existing output)
.PARAMETER Tag
    Git tag to create (e.g. v0.0.0.1). If omitted, no tag is created.
#>
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [string]$Tag
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# ── 1. Read release version from version.txt ──
$versionFile = Join-Path $root 'version.txt'
if (-not (Test-Path $versionFile)) {
    Write-Error "version.txt not found at $versionFile"
    exit 1
}

$releaseVersion = (Get-Content $versionFile -Raw).Trim()
$parts = $releaseVersion -split '\.'
if ($parts.Count -ne 4) {
    Write-Error "Invalid version format in version.txt: '$releaseVersion'. Expected: Major.Minor.Build.Revision"
    exit 1
}

Write-Host "Release version: $releaseVersion" -ForegroundColor Cyan

# ── 2. Patch Package.appxmanifest ──
$manifestPath = Join-Path $root 'dosbox-uwp\Package.appxmanifest'
$manifest = Get-Content $manifestPath -Raw
$manifest = $manifest -replace '(<Identity[^>]+Version=")[^"]*(")', "`${1}$releaseVersion`${2}"
Set-Content -Path $manifestPath -Value $manifest -NoNewline
Write-Host "Patched appxmanifest: $releaseVersion" -ForegroundColor Green

# ── 3. Build ──
if (-not $SkipBuild) {
    Write-Host "`n=== Building $Configuration|x64 ===" -ForegroundColor Cyan
    & "$PSScriptRoot\build.ps1" -Configuration $Configuration -Platform x64
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit $LASTEXITCODE }
}

# ── 4. Locate MSIX output ──
$platform = 'x64'
$configSuffix = if ($Configuration -eq 'Debug') { '_Debug' } else { '' }

$pkgRoot = Join-Path $root "AppPackages\dosbox-uwp"
$msix = $null
$pkgDir = $null

$msixDirs = @(
    "dosbox-uwp_${releaseVersion}_${platform}${configSuffix}_Test",
    "dosbox-uwp_${releaseVersion}_${platform}_Test",
    "dosbox-uwp_${releaseVersion}_${platform}${configSuffix}"
)
foreach ($d in $msixDirs) {
    $candidate = Join-Path $pkgRoot $d
    if (Test-Path $candidate) {
        $msixFile = Get-ChildItem $candidate -Filter "*.msix" | Select-Object -First 1
        if ($msixFile) { $msix = $msixFile.FullName; $pkgDir = $candidate; break }
    }
}

# Fallback: x64 output layout
if (-not $msix) {
    $layoutDir = Join-Path $root "x64\$Configuration\dosbox-uwp"
    $candidate = Join-Path $layoutDir "dosbox-uwp.msix"
    if (Test-Path $candidate) { $msix = $candidate; $pkgDir = $layoutDir }
}

if (-not $msix) {
    Write-Error "MSIX not found after build."
    exit 1
}
Write-Host "`nMSIX: $msix" -ForegroundColor Green

# ── 5. Collect distributable files (MSIX + x64 dependencies only) ──
$distribDir = Join-Path $root "distribute"
if (Test-Path $distribDir) { Remove-Item $distribDir -Recurse -Force }
New-Item -ItemType Directory -Path $distribDir -Force | Out-Null

# Copy MSIX
$msixName = Split-Path $msix -Leaf
Copy-Item $msix (Join-Path $distribDir $msixName)
Write-Host "  $msixName" -ForegroundColor Gray

# Copy x64 dependencies only
$depDir = Join-Path $pkgDir 'Dependencies'
if (Test-Path $depDir) {
    $x64DepDir = Join-Path $depDir 'x64'
    if (Test-Path $x64DepDir) {
        $destDepDir = Join-Path $distribDir 'Dependencies\x64'
        New-Item -ItemType Directory -Path $destDepDir -Force | Out-Null
        Get-ChildItem $x64DepDir -Filter "*.appx" | ForEach-Object {
            Copy-Item $_.FullName (Join-Path $destDepDir $_.Name)
            $size = [math]::Round($_.Length / 1KB)
            Write-Host "  Dependencies\x64\$($_.Name) ($size KB)" -ForegroundColor Gray
        }
    }
}

# Copy Install.ps1 (patched with release version)
$installSrc = Join-Path $PSScriptRoot 'install.ps1'
if (Test-Path $installSrc) {
    $installContent = Get-Content $installSrc -Raw
    $installContent = $installContent -replace '_1\.0\.0\.0\.', "_${releaseVersion}."
    Set-Content (Join-Path $distribDir 'Install.ps1') $installContent -NoNewline
    Write-Host "  Install.ps1" -ForegroundColor Gray
}

# Copy cert
$cerPath = Join-Path $root 'certs\dosbox-uwp.cer'
if (Test-Path $cerPath) {
    Copy-Item $cerPath (Join-Path $distribDir 'dosbox-uwp.cer')
    Write-Host "  dosbox-uwp.cer" -ForegroundColor Gray
}

# ── 6. Create distributable zip ──
$zipName = "dosbox-uwp_${releaseVersion}_x64.zip"
$zipPath = Join-Path $root $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

Compress-Archive -Path (Join-Path $distribDir '*') -DestinationPath $zipPath -Force
$zipSize = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Host "`n=== Distributable ===" -ForegroundColor Green
Write-Host "  $zipName ($zipSize MB)" -ForegroundColor Green
Write-Host "  $zipPath" -ForegroundColor Gray

# ── 7. Git tag (optional) ──
if ($Tag) {
    $tagName = if ($Tag -notlike 'v*') { "v$Tag" } else { $Tag }
    Write-Host "`n=== Tagging $tagName ===" -ForegroundColor Cyan
    & git -C $root tag -a $tagName -m "Release $releaseVersion"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  Created tag: $tagName" -ForegroundColor Green
        Write-Host "  Push with: git push origin $tagName" -ForegroundColor Yellow
    } else {
        Write-Warning "Tag creation failed. Tag manually: git tag -a $tagName -m 'Release $releaseVersion'"
    }
}

# ── 8. Increment version.txt for next snapshot ──
$parts[3] = [int]$parts[3] + 1
$nextVersion = $parts -join '.'
Set-Content -Path $versionFile -Value "$nextVersion`n" -NoNewline
Write-Host "`n=== Version bumped: $releaseVersion -> $nextVersion ===" -ForegroundColor Cyan
Write-Host "  version.txt updated to $nextVersion (next snapshot)" -ForegroundColor Gray

# ── 9. Summary ──
Write-Host "`n=== Release $releaseVersion complete ===" -ForegroundColor Green
Write-Host "  Released: $releaseVersion" -ForegroundColor Cyan
Write-Host "  Next:     $nextVersion" -ForegroundColor Gray
Write-Host "  MSIX:     $msix" -ForegroundColor Gray
Write-Host "  Zip:      $zipPath" -ForegroundColor Gray
if (-not $Tag) {
    Write-Host "  Tag:      (skipped — use -Tag v$releaseVersion to create)" -ForegroundColor Gray
}
Write-Host ""
