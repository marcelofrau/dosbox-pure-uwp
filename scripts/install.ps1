param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('x64')][string]$Platform = 'x64'
)

$root = Split-Path -Parent $PSScriptRoot

# Read version from version.txt
$versionFile = Join-Path $root 'version.txt'
if (-not (Test-Path $versionFile)) { Write-Error "version.txt not found"; exit 1 }
$version = (Get-Content $versionFile -Raw).Trim()

$pkgName = '81ef2129-2f08-4d22-9816-050b2b62b308'

$pkgDir = Join-Path $root "AppPackages\dosbox-uwp\dosbox-uwp_${version}_${Platform}_${Configuration}_Test"
$msix = Join-Path $pkgDir "dosbox-uwp_${version}_${Platform}_${Configuration}.msix"
$extractDir = Join-Path $pkgDir "extracted"

# Fallback: glob for any recent MSIX
if (-not (Test-Path $msix)) {
    $pkgRoot = Join-Path $root "AppPackages\dosbox-uwp"
    $found = Get-ChildItem $pkgRoot -Directory -Filter "dosbox-uwp_*_${Platform}_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($found) {
        $f = Get-ChildItem $found.FullName -Filter "*.msix" | Select-Object -First 1
        if ($f) {
            $msix = $f.FullName
            $pkgDir = $found.FullName
            $extractDir = Join-Path $pkgDir "extracted"
            # Extract version from filename
            if ($f.Name -match 'dosbox-uwp_(\d+\.\d+\.\d+\.\d+)_') { $version = $Matches[1] }
        }
    }
}

if (-not (Test-Path $msix)) {
    Write-Error "Package not found at $msix. Build first."
    exit 1
}

# Trust the code-signing cert — needed for runtime trust
$cerPath = Join-Path $root "certs\dosbox-uwp.cer"
$certThumbprint = 'F968DA788741D89E236A777BA5E7B8BDBC7429CB'
foreach ($store in @('Root', 'TrustedPeople')) {
    $certStore = "Cert:\CurrentUser\$store"
    $existing = Get-ChildItem $certStore -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $certThumbprint }
    if (-not $existing -and (Test-Path $cerPath)) {
        Write-Host "Installing cert to CurrentUser\$store..." -ForegroundColor Cyan
        Import-Certificate -FilePath $cerPath -CertStoreLocation $certStore -ErrorAction Stop | Out-Null
    }
}

Write-Host "Uninstalling old package (if any)..." -ForegroundColor Cyan
$pkg = Get-AppxPackage -Name $pkgName -ErrorAction SilentlyContinue
if ($pkg) {
    Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction Stop
    Write-Host "  Removed: $($pkg.PackageFullName)" -ForegroundColor DarkGray
}

# Extract MSIX and register from unpacked dir.
# Add-AppxPackage on the .msix directly fails with 0x800B0109 cert trust
# even when cert is in Root + TrustedPeople (Windows 11 bug?).
# -Register bypasses the cert signature check.
Write-Host "Installing $msix (extract + register)..." -ForegroundColor Cyan
& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\MakeAppx.exe" unpack /p $msix /d $extractDir /o 2>&1 | Out-Null
if (-not (Test-Path (Join-Path $extractDir "AppxManifest.xml"))) {
    Write-Error "Extract failed — AppxManifest.xml not found."
    exit 1
}
Add-AppxPackage -Register (Join-Path $extractDir "AppxManifest.xml") -ErrorAction Stop
Write-Host "Install done." -ForegroundColor Green
