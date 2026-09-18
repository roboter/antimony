<#
.SYNOPSIS
Simulates the GitHub Actions Windows build locally.

.DESCRIPTION
This script executes the exact commands found in .github/workflows/windows-build.yml
inside your local MSYS2 UCRT64 environment.
#>

$msys2Path = "C:\msys64"
$msys2Shell = Join-Path $msys2Path "msys2_shell.cmd"

if (-Not (Test-Path $msys2Shell)) {
    Write-Host "MSYS2 was not found at $msys2Path." -ForegroundColor Red
    exit 1
}

$repoRoot = (Get-Item $PSScriptRoot).Parent.FullName
$msysRepoRoot = $repoRoot -replace '\\', '/'
$msysRepoRoot = $msysRepoRoot -replace '^([a-zA-Z]):', '/$1'

Write-Host "==> Step 1: Installing Dependencies (Pacman)" -ForegroundColor Cyan
$pacmanCmd = "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-qt5 mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-libpng"
& $msys2Shell -ucrt64 -defterm -no-start -c $pacmanCmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> Step 2: Building Antimony" -ForegroundColor Cyan
$buildCmd = "cd '$msysRepoRoot' && mkdir -p build && cd build && qmake-qt5 ../qt/antimony.pro && mingw32-make -j`$(nproc) && mkdir -p release/sb && cp -f -R ../py/nodes release/sb/ && cp -f -R ../py/fab release/sb/"
& $msys2Shell -ucrt64 -defterm -no-start -c $buildCmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> Step 3: Building and Running Tests" -ForegroundColor Cyan
$testCmd = "cd '$msysRepoRoot' && mkdir -p build-tests && cd build-tests && qmake-qt5 ../qt/antimony-tests.pro && mingw32-make -j`$(nproc) && if [ -f `"release/antimony-tests.exe`" ]; then ./release/antimony-tests.exe; else ./antimony-tests.exe; fi"
& $msys2Shell -ucrt64 -defterm -no-start -c $testCmd
if ($LASTEXITCODE -ne 0) { 
    Write-Host "`n[!] Tests failed or failed to build." -ForegroundColor Red
    exit $LASTEXITCODE 
}

Write-Host "`n==> CI simulation completed successfully!" -ForegroundColor Green
