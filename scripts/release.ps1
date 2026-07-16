param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [string]$Tag
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# ── 1. Build (PreBuildEvent increments version) ──
if (-not $SkipBuild) {
    Write-Host "`n=== Building $Configuration|x64 ===" -ForegroundColor Cyan
    & "$PSScriptRoot\build.ps1" -Configuration $Configuration -Platform x64
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit $LASTEXITCODE }
}

# ── 2. Read version AFTER build (PreBuildEvent wrote it) ──
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

# ── 3. Locate MSIX output ──
$platform = 'x64'
$configSuffix = if ($Configuration -eq 'Debug') { '_Debug' } else { '' }

$pkgRoot = Join-Path $root "AppPackages\dosbox-uwp"
$msix = $null
$pkgDir = $null

# Try exact version match
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

# Fallback: glob for any recent dosbox-uwp MSIX package dir
if (-not $msix) {
    $found = Get-ChildItem $pkgRoot -Directory -Filter "dosbox-uwp_*_x64_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($found) {
        $msixFile = Get-ChildItem $found.FullName -Filter "*.msix" | Select-Object -First 1
        if ($msixFile) { $msix = $msixFile.FullName; $pkgDir = $found.FullName }
    }
}

# Final fallback: x64 output layout
if (-not $msix) {
    $layoutDir = Join-Path $root "x64\$Configuration\dosbox-uwp"
    $candidate = Join-Path $layoutDir "dosbox-uwp.msix"
    if (Test-Path $candidate) { $msix = $candidate; $pkgDir = $layoutDir }
}

if (-not $msix) {
    Write-Error "MSIX not found after build."
    exit 1
}

# Extract actual version from MSIX filename if different
$msixName = Split-Path $msix -Leaf
if ($msixName -match 'dosbox-uwp_(\d+\.\d+\.\d+\.\d+)_') {
    $actualVer = $Matches[1]
    if ($actualVer -ne $releaseVersion) {
        Write-Host "MSIX version ($actualVer) differs from version.txt ($releaseVersion) — using MSIX version" -ForegroundColor Yellow
        $releaseVersion = $actualVer
        $parts = $releaseVersion -split '.'
    }
}

Write-Host "`nMSIX: $msix" -ForegroundColor Green

# ── 4. Collect distributable files ──
$distribDir = Join-Path $root "distribute"
if (Test-Path $distribDir) { Remove-Item $distribDir -Recurse -Force }
New-Item -ItemType Directory -Path $distribDir -Force | Out-Null

# Copy MSIX
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

# ── 5. Create distributable zip ──
$zipName = "dosbox-uwp_${releaseVersion}_x64.zip"
$zipPath = Join-Path $root $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

Compress-Archive -Path (Join-Path $distribDir '*') -DestinationPath $zipPath -Force
$zipSize = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Host "`n=== Distributable ===" -ForegroundColor Green
Write-Host "  $zipName ($zipSize MB)" -ForegroundColor Green
Write-Host "  $zipPath" -ForegroundColor Gray

# ── 6. Git tag (optional) ──
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

# ── 7. Summary ──
Write-Host "`n=== Release $releaseVersion complete ===" -ForegroundColor Green
Write-Host "  Released: $releaseVersion" -ForegroundColor Cyan
Write-Host "  MSIX:     $msix" -ForegroundColor Gray
Write-Host "  Zip:      $zipPath" -ForegroundColor Gray
if (-not $Tag) {
    Write-Host "  Tag:      (skipped — use -Tag v$releaseVersion to create)" -ForegroundColor Gray
}
Write-Host ""
