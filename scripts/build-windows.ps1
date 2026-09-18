<#
.SYNOPSIS
Builds the Antimony project on Windows using MSYS2 UCRT64.

.DESCRIPTION
This script checks if MSYS2 is installed. If so, it uses the UCRT64 environment
to install dependencies via pacman, and then configures and builds the project
using qmake and make.
#>

$msys2Path = "C:\msys64"
$msys2Shell = Join-Path $msys2Path "msys2_shell.cmd"

if (-Not (Test-Path $msys2Shell)) {
    Write-Host "MSYS2 was not found at $msys2Path." -ForegroundColor Red
    Write-Host "Please install MSYS2 from https://www.msys2.org/ and install it to C:\msys64" -ForegroundColor Yellow
    exit 1
}

$repoRoot = (Get-Item $PSScriptRoot).Parent.FullName

# We need to run commands inside the MSYS2 UCRT64 environment.
Write-Host "Installing dependencies using pacman..." -ForegroundColor Cyan
$pacmanCmd = "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-qt5 mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-libpng"
& $msys2Shell -ucrt64 -defterm -no-start -c $pacmanCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to install dependencies." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Building Antimony..." -ForegroundColor Cyan
# Convert Windows path to MSYS path
$msysRepoRoot = $repoRoot -replace '\\', '/'
$msysRepoRoot = $msysRepoRoot -replace '^([a-zA-Z]):', '/$1'

$buildCmd = "mkdir -p '$msysRepoRoot/build' && cd '$msysRepoRoot/build' && qmake-qt5 ../qt/antimony.pro && mingw32-make -j8"
& $msys2Shell -ucrt64 -defterm -no-start -c $buildCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Copying python modules to release directory..." -ForegroundColor Cyan
$sbTarget = "$msysRepoRoot/build/release/sb"
$copyCmd = "mkdir -p '$sbTarget' && cp -f -R '$msysRepoRoot/py/nodes' '$sbTarget/' && cp -f -R '$msysRepoRoot/py/fab' '$sbTarget/'"
& $msys2Shell -ucrt64 -defterm -no-start -c $copyCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to copy python modules." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "You can run Antimony from: $repoRoot\build\antimony.exe" -ForegroundColor Green
