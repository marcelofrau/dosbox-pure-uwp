param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateSet('x64')][string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

# ── Read version ──
$versionFile = Join-Path $PSScriptRoot '..\version.txt'
if (!(Test-Path $versionFile)) { Write-Error "version.txt not found"; exit 1 }
$version = (Get-Content $versionFile -Raw).Trim()

# ── Load .env ──
$envPath = Join-Path $PSScriptRoot '..\.env'
if (!(Test-Path $envPath)) { Write-Error ".env not found at $envPath"; exit 1 }
Get-Content $envPath | ForEach-Object {
    $kv = $_ -split '=', 2
    if ($kv[0]) { Set-Item -Path "env:$($kv[0])" -Value $kv[1] }
}
$xboxIp = $env:XBOX_IP
$xboxUser = $env:XBOX_USER
$xboxPass = $env:XBOX_PASS
if (!$xboxIp -or !$xboxUser -or !$xboxPass) {
    Write-Error ".env missing XBOX_IP, XBOX_USER, or XBOX_PASS"; exit 1
}

$base64 = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("${xboxUser}:${xboxPass}"))
$headers = @{ Authorization = "Basic $base64" }
$baseUri = "https://${xboxIp}:11443"

# ── Build (PreBuildEvent increments version) ──
$root = Split-Path -Parent $PSScriptRoot
$sln = Join-Path $root 'dosbox-pure-unleashed-uwp.sln'
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'

Write-Host "=== Build $Configuration|x64 ===" -ForegroundColor Cyan
& $msbuild $sln /p:Configuration=$Configuration /p:Platform=$Platform /nowarn:MSB4011 /nologo
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# ── Read version AFTER build ──
$version = (Get-Content $versionFile -Raw).Trim()

# ── Locate MSIX ──
$configSuffix = if ($Configuration -eq 'Debug') { '_Debug' } else { '' }
$pkgDir = Join-Path $root "AppPackages\dosbox-uwp\dosbox-uwp_${version}_${Platform}${configSuffix}_Test"
$msix = Join-Path $pkgDir "dosbox-uwp_${version}_${Platform}${configSuffix}.msix"

# Fallback: glob
if (!(Test-Path $msix)) {
    $found = Get-ChildItem (Join-Path $root "AppPackages\dosbox-uwp") -Directory -Filter "dosbox-uwp_*_${Platform}_*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($found) {
        $f = Get-ChildItem $found.FullName -Filter "*.msix" | Select-Object -First 1
        if ($f) { $msix = $f.FullName; $pkgDir = $found.FullName }
    }
}

if (!(Test-Path $msix)) { Write-Error "MSIX not found after build"; exit 1 }
Write-Host "MSIX: $msix" -ForegroundColor Cyan

# ── Upload via WDP REST API ──
Write-Host "=== Deploy to $xboxIp ===" -ForegroundColor Cyan
try {
    Invoke-RestMethod -Uri "${baseUri}/api/app/packagemanager/package" -Method POST -Headers $headers `
        -Form @{ package = Get-Item -LiteralPath $msix } -SkipCertificateCheck | Out-Null
    Write-Host "Deploy OK" -ForegroundColor Green
} catch {
    Write-Error "Deploy failed: $_"
    Write-Host "`nCheck:"
    Write-Host "  - Xbox Dev Mode: ON"
    Write-Host "  - Dev Mode Home → Remote Access: enabled"
    Write-Host "  - PC & Xbox same network"
    Write-Host "  - Test: curl -k ${baseUri}/api/os/info"
    exit 1
}

# ── Find installed package and launch ──
Write-Host "=== Launch ===" -ForegroundColor Cyan
try {
    $installed = Invoke-RestMethod -Uri "${baseUri}/api/app/packagemanager/packages" `
        -Method GET -Headers $headers -SkipCertificateCheck
    $pkg = $installed.Packages | Where-Object { $_.PackageFamilyName -like "*81ef2129*" } | Select-Object -First 1
    if (!$pkg) {
        Write-Warning "Package installed but not found via API. Start manually."
        Write-Host "Manual: Xbox Dev Mode Home → your tile → launch"
        exit 0
    }
    $launchBody = @{
        AppId = "App"
        PackageFamilyName = $pkg.PackageFamilyName
    } | ConvertTo-Json
    Invoke-RestMethod -Uri "${baseUri}/api/taskmanager/app" -Method POST -Headers $headers `
        -Body $launchBody -ContentType "application/json" -SkipCertificateCheck | Out-Null
    Write-Host "Launched: $($pkg.PackageFamilyName)" -ForegroundColor Green
} catch {
    Write-Warning "Launch failed: $_"
    Write-Host "Manual: Xbox Dev Mode Home → your tile → launch"
}
