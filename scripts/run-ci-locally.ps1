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
$pacmanCmd = "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-qt5 mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja flex lemon"
& $msys2Shell -ucrt64 -defterm -no-start -c $pacmanCmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> Step 2: Building Antimony" -ForegroundColor Cyan
$buildCmd = "cd '$msysRepoRoot' && mkdir -p build && cd build && cmake -G Ninja .. && ninja"
& $msys2Shell -ucrt64 -defterm -no-start -c $buildCmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`n==> CI simulation completed successfully!" -ForegroundColor Green
Write-Host "NOTE: Test execution has been removed as the test suite is not currently compatible with CMake." -ForegroundColor Yellow
