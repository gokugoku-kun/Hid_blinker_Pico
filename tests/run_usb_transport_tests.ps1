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

# A blocking regression must fail the suite instead of hanging the runner.
$stdoutPath = Join-Path $outputDirectory "usb_transport.stdout.txt"
$stderrPath = Join-Path $outputDirectory "usb_transport.stderr.txt"
$testProcess = Start-Process -FilePath $executable -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
if (-not $testProcess.WaitForExit(15000)) {
    $testProcess.Kill()
    $testProcess.WaitForExit()
    throw "USB transport tests exceeded 15 seconds; a call may be blocking."
}
$testProcess.WaitForExit()
$testProcess.Refresh()
Get-Content -LiteralPath $stdoutPath
Get-Content -LiteralPath $stderrPath
if ($testProcess.ExitCode -ne 0) {
    throw "USB transport tests failed (exit $($testProcess.ExitCode))."
}
