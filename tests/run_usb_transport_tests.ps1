param(
    [string]$Compiler = "C:/MinGW/bin/gcc.exe"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$compilerCommand = Get-Command $Compiler -CommandType Application -ErrorAction Stop
if ([IO.Path]::GetFileName($compilerCommand.Source) -like "arm-none-eabi-*") {
    throw "Select a native host compiler, not the Pico cross compiler."
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareDirectory = Join-Path $projectRoot "Hid_blinker_Pico"
$outputDirectory = Join-Path $firmwareDirectory "build/host-tests"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$executable = Join-Path $outputDirectory "test_usb_transport.exe"
$compilerArguments = @(
    "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
    "-I", (Join-Path $PSScriptRoot "stubs"),
    "-I", $firmwareDirectory,
    (Join-Path $PSScriptRoot "test_usb_transport.c"),
    (Join-Path $firmwareDirectory "usb_transport.c"),
    "-o", $executable
)

Write-Host "Building USB transport host tests with $($compilerCommand.Source)"
& $compilerCommand.Source @compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "USB transport test compilation failed (exit $LASTEXITCODE)."
}

# The suite contains only non-blocking host checks; invoke it directly so
# PowerShell preserves the native process exit code on Windows PowerShell.
& $executable
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "USB transport tests failed (exit $exitCode)."
}
