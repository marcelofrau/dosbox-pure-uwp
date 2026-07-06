$WC   = "C:\Apps\OW\binnt\wcc.exe"
$WL   = "C:\Apps\OW\binnt\wlink.exe"
$INC  = "-I=C:\Apps\OW\h"
$LIB  = "C:\Apps\OW\lib286\dos"
$OUT  = "dist"
$DEST = "E:\PC\DOSBoxPure\inputtest-better.dosz"

$env:WATCOM = "C:\Apps\OW"
$env:PATH   = "C:\Apps\OW\binnt;$env:PATH"

if (!(Test-Path $OUT)) { mkdir $OUT | Out-Null }

function Compile($src) {
    Write-Host "==> wcc $src.c"
    & $WC -ms $INC "$src.c"
    if ($LASTEXITCODE -ne 0) { Write-Host "FAIL $src.c"; exit 1 }
    Move-Item "$src.obj" "$OUT\$src.obj" -Force
}

function Link($src) {
    Write-Host "==> wlink $src.exe"
    & $WL option quiet file "$OUT\$src.obj" name "$OUT\$src.exe" `
         library clibs.lib library graph.lib libpath $LIB format dos
    if ($LASTEXITCODE -ne 0) { Write-Host "FAIL $src.exe"; exit 1 }
}

Compile "hello"
Link    "hello"

Compile "inputtest"
Link    "inputtest"

Write-Host "==> copy inputtest.com from sibling"
Copy-Item "..\inputtest\inputtest.com" "$OUT\inputtest.com" -Force

Write-Host "==> zip .dosz"
Compress-Archive -Path "$OUT\hello.exe", "$OUT\inputtest.exe", "$OUT\inputtest.com" -DestinationPath "$OUT\inputtest-better.dosz" -Force

Write-Host "==> copy to $DEST"
Copy-Item "$OUT\inputtest-better.dosz" $DEST -Force
Copy-Item "$OUT\inputtest.com" "E:\PC\DOSBoxPure\inputtest.com" -Force

Write-Host "OK - dist\inputtest-better.dosz + $DEST"
