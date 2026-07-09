param(
    [string]$PackageFamily = ""
)

if (-not $PackageFamily) {
    $pkg = Get-AppxPackage -Name "*dosbox*" -User $env:USERNAME | Select-Object -First 1
    if (-not $pkg) {
        $pkg = Get-AppxPackage -Name "*dosbox*" | Select-Object -First 1
    }
    if ($pkg) {
        $PackageFamily = $pkg.PackageFamilyName
    }
}

if (-not $PackageFamily) {
    Write-Host "Package not found. Provide PackageFamilyName manually:" -ForegroundColor Yellow
    Write-Host "  .\loopback-exempt.ps1 -PackageFamily ""81ef2129-2f08-4d22-9816-050b2b62b308_atgxky5qxrpe0""" -ForegroundColor Cyan
    exit 1
}

Write-Host "Exempting loopback for: $PackageFamily" -ForegroundColor Cyan
$result = CheckNetIsolation.exe LoopbackExempt -is -n="$PackageFamily" 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "OK — loopback exempt" -ForegroundColor Green
} else {
    Write-Host "FAIL — need admin?" -ForegroundColor Red
    Write-Host "$result" -ForegroundColor Red
}
