param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('x64')][string]$Platform = 'x64',
    [switch]$SkipBuild
)

$root = Split-Path -Parent $PSScriptRoot

# ensure signing certificate
$pfx = Join-Path $root 'certs\dosbox-uwp.pfx'
$cerPath = Join-Path $root 'certs\dosbox-uwp.cer'
$certPass = 'dev'
if (-not (Test-Path $pfx)) {
    Write-Host "Creating signing certificate ..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path (Split-Path $pfx -Parent) -Force | Out-Null
    $secPass = ConvertTo-SecureString $certPass -AsPlainText -Force
    $cert = New-SelfSignedCertificate -Subject "CN=Marcelo Frau" -FriendlyName "dosbox-uwp" `
        -Type CodeSigningCert -KeyUsage DigitalSignature `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") `
        -CertStoreLocation Cert:\CurrentUser\My
    Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $secPass -Force | Out-Null
    Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT -Force | Out-Null
    Write-Host "  Cert: $pfx (thumbprint: $($cert.Thumbprint))" -ForegroundColor Green
    Write-Host "  IMPORTANT: Update PackageCertificateThumbprint in .vcxproj to $($cert.Thumbprint)" -ForegroundColor Yellow
}

# build (PreBuildEvent increments version)
if (-not $SkipBuild) {
    & "$PSScriptRoot\build.ps1" -Configuration $Configuration -Platform $Platform
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Read version AFTER build (PreBuildEvent wrote it)
$versionFile = Join-Path $root 'version.txt'
if (-not (Test-Path $versionFile)) { Write-Error "version.txt not found"; exit 1 }
$version = (Get-Content $versionFile -Raw).Trim()

# locate msix
$pkgRoot = Join-Path $root "AppPackages\dosbox-uwp"
$msix = $null
$pkgDir = $null

$msixDirs = @(
    "dosbox-uwp_${version}_${Platform}_${Configuration}_Test",
    "dosbox-uwp_${version}_${Platform}_Test",
    "dosbox-uwp_${version}_${Platform}_${Configuration}"
)
foreach ($d in $msixDirs) {
    $candidate = Join-Path $pkgRoot "$d\dosbox-uwp_${version}_${Platform}.msix"
    if (Test-Path $candidate) { $msix = $candidate; $pkgDir = Join-Path $pkgRoot $d; break }
    $candidate2 = Join-Path $pkgRoot "$d\dosbox-uwp_${version}_${Platform}_${Configuration}.msix"
    if (Test-Path $candidate2) { $msix = $candidate2; $pkgDir = Join-Path $pkgRoot $d; break }
}

# Fallback: glob for any recent MSIX package dir
if (-not $msix) {
    $found = Get-ChildItem $pkgRoot -Directory -Filter "dosbox-uwp_*_${Platform}_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($found) {
        $f = Get-ChildItem $found.FullName -Filter "*.msix" | Select-Object -First 1
        if ($f) { $msix = $f.FullName; $pkgDir = $found.FullName }
    }
}

# Final fallback: x64 output layout
if (-not $msix) {
    $layoutDir = Join-Path $root "x64\$Configuration\dosbox-uwp"
    $candidate = Join-Path $layoutDir "dosbox-uwp.msix"
    if (Test-Path $candidate) { $msix = $candidate; $pkgDir = $layoutDir }
    else {
        Write-Error "MSIX not found."
        exit 1
    }
}

Write-Host "`n=== Generated files ===" -ForegroundColor Green
Write-Host "  MSIX:       $msix" -ForegroundColor Green
Write-Host "  INSTALL:    $pkgDir\Install.ps1" -ForegroundColor Green

$depDir = Join-Path $pkgDir 'Dependencies'
if (Test-Path $depDir) {
    Write-Host "`n=== Dependencies ===" -ForegroundColor Cyan
    Get-ChildItem $depDir -Recurse -File | ForEach-Object {
        $size = [math]::Round($_.Length / 1KB)
        Write-Host "  $($_.Name)  ($size KB)" -ForegroundColor Gray
    }
}

Write-Host "`n=== Deploy on Xbox ===" -ForegroundColor Cyan
Write-Host "  1. Copy $pkgDir to Xbox" -ForegroundColor Gray
Write-Host "  2. Run Install.ps1 (installs cert + deps + app)" -ForegroundColor Gray
Write-Host ""
