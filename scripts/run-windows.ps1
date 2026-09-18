<#
.SYNOPSIS
Runs the built Antimony executable on Windows, ensuring MSYS2 DLLs are in the PATH.

.DESCRIPTION
Adds the MSYS2 UCRT64 bin directory to the environment PATH so that dynamically
linked libraries (like libboost_python314-mt.dll, Qt DLLs, etc.) can be found
at runtime, and then launches Antimony.
#>

$msys2BinPath = "C:\msys64\ucrt64\bin"

if (-Not (Test-Path $msys2BinPath)) {
    Write-Host "MSYS2 UCRT64 bin directory not found at $msys2BinPath." -ForegroundColor Red
    exit 1
}

# Add MSYS2 bin to PATH for this session only
$env:PATH = "$msys2BinPath;" + $env:PATH

$repoRoot = (Get-Item $PSScriptRoot).Parent.FullName
$exePath = Join-Path $repoRoot "build\app\antimony.exe"

if (-Not (Test-Path $exePath)) {
    Write-Host "Antimony executable not found at $exePath." -ForegroundColor Red
    Write-Host "Please run scripts\build-windows.ps1 first." -ForegroundColor Yellow
    exit 1
}

Write-Host "Launching Antimony..." -ForegroundColor Cyan
Start-Process -FilePath $exePath -NoNewWindow -Wait
