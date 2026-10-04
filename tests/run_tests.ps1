param(
    [string]$Compiler = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Use a native host compiler, not the Pico's arm-none-eabi compiler.
# Example: ./tests/run_tests.ps1 -Compiler C:/MinGW/bin/gcc.exe
$compilerCandidates = @()
if ($Compiler) {
    $compilerCandidates += $Compiler
} else {
    if ($env:CC) { $compilerCandidates += $env:CC }
    $compilerCandidates += @("C:/MinGW/bin/gcc.exe", "clang", "gcc", "cc")
}

$compilerPath = $null
foreach ($candidate in $compilerCandidates) {
    $command = Get-Command $candidate -CommandType Application -ErrorAction SilentlyContinue
    if ($command) {
        $compilerPath = $command.Source
        break
    }
}
if (-not $compilerPath) {
    throw "A native GCC or Clang compiler is required. Pass its executable with -Compiler."
}
if ([IO.Path]::GetFileName($compilerPath) -like "arm-none-eabi-*") {
    throw "The Pico cross compiler cannot produce runnable host tests. Select native GCC or Clang."
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$firmwareDirectory = Join-Path $projectRoot "Hid_blinker_Pico"
$outputDirectory = Join-Path $firmwareDirectory "build/host-tests"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$executable = Join-Path $outputDirectory "test_firmware.exe"
$compilerArguments = @(
    "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
    "-I", $firmwareDirectory,
    (Join-Path $PSScriptRoot "test_firmware.c"),
    (Join-Path $firmwareDirectory "led_controller.c"),
    (Join-Path $firmwareDirectory "led_protocol.c"),
    "-o", $executable
)

Write-Host "Building host firmware tests with $compilerPath"
& $compilerPath @compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Host test compilation failed (exit $LASTEXITCODE)."
}

& $executable
if ($LASTEXITCODE -ne 0) {
    throw "Host firmware tests failed (exit $LASTEXITCODE)."
}
