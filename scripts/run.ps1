param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateSet('x64')][string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sln = Join-Path $root 'dosbox-pure-unleashed-uwp.sln'

$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'

Write-Host '=== Build ===' -ForegroundColor Cyan
& $msbuild $sln /p:Configuration=$Configuration /p:Platform=$Platform /nowarn:MSB4011

# Read version AFTER build
$versionFile = Join-Path $root 'version.txt'
$version = (Get-Content $versionFile -Raw).Trim()

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

$extractDir = Join-Path $env:TEMP "dosbox-uwp-${Configuration}"
if (Test-Path $extractDir) { Remove-Item $extractDir -Recurse -Force; Start-Sleep -Milliseconds 200 }
New-Item -ItemType Directory -Path $extractDir -Force | Out-Null

Write-Host "=== Extract $msix ===" -ForegroundColor Cyan
& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\MakeAppx.exe" unpack /p $msix /d $extractDir /o
if ($LASTEXITCODE -ne 0) { Write-Error "Extract failed"; exit 1 }

Write-Host "=== Register ===" -ForegroundColor Cyan
$pkgName = '81ef2129-2f08-4d22-9816-050b2b62b308'
$old = Get-AppxPackage -Name $pkgName
if ($old) { Remove-AppxPackage $old.PackageFullName }
Add-AppxPackage -Register (Join-Path $extractDir 'AppxManifest.xml')

Write-Host "=== Launch ===" -ForegroundColor Cyan
Add-Type -AssemblyName System.Runtime.InteropServices

$t = @"
using System;
using System.Runtime.InteropServices;

[ComImport, Guid("2e941141-7f97-4756-ba1d-9decde894a3d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IApplicationActivationManager {
    int ActivateApplication(string appUserModelId, string arguments, uint options, out uint processId);
    int ActivateForFile(string appUserModelId, IntPtr itemArray, string verb, out uint processId);
    int ActivateForProtocol(string appUserModelId, IntPtr itemArray, out uint processId);
}

public class AppActivator {
    static readonly Guid CLSID = new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C");
    public static uint Launch(string aumid) {
        var type = Type.GetTypeFromCLSID(CLSID);
        var mgr = (IApplicationActivationManager)Activator.CreateInstance(type);
        uint pid;
        int hr = mgr.ActivateApplication(aumid, null, 0, out pid);
        if (hr < 0) throw new COMException("ActivateApplication", hr);
        return pid;
    }
}
"@
Add-Type -TypeDefinition $t
$pfn = (Get-AppxPackage -Name $pkgName).PackageFamilyName
$aumid = "${pfn}!App"
$procId = [AppActivator]::Launch($aumid)
Write-Host "Launched PID=$procId" -ForegroundColor Green
