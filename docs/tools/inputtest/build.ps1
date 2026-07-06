param()

$asm = "inputtest.asm"
$com = "inputtest.com"
$dosz = "inputtest.dosz"

$nasm = Get-Command nasm -ErrorAction SilentlyContinue
if (-not $nasm) {
    Write-Error "NASM not found in PATH. Install NASM or add it to PATH."
    exit 1
}

Write-Host "Assembling $asm..."
& $nasm.Source -f bin -o $com $asm
if ($LASTEXITCODE -ne 0) {
    Write-Error "Assembly failed."
    exit $LASTEXITCODE
}

if (Test-Path $dosz) {
    Remove-Item $dosz -Force
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$tempDir = Join-Path $env:TEMP ("inputtest-build-" + [System.Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $tempDir | Out-Null
Copy-Item $com -Destination (Join-Path $tempDir "inputtest.com")
[IO.Compression.ZipFile]::CreateFromDirectory($tempDir, $dosz)
Remove-Item $tempDir -Recurse -Force

Write-Host "Built $com and packaged $dosz."
