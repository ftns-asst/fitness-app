[CmdletBinding()]
param(
    [string]$ClangFormat = "",
    [switch]$Check
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$versionFile = Join-Path (Split-Path $projectRoot -Parent) ".clang-format-version"
$expectedVersion = (Get-Content -LiteralPath $versionFile -Raw).Trim()
if ($expectedVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "Invalid clang-format version in $versionFile"
}

if (-not $ClangFormat -and $env:CLANG_FORMAT) {
    $ClangFormat = $env:CLANG_FORMAT
}

$candidates = if ($ClangFormat) {
    @($ClangFormat)
} else {
    @(
        "clang-format",
        "clang-format-18",
        (Join-Path $projectRoot "build/tools/clang-format-$expectedVersion/clang_format/data/bin/clang-format.exe")
    )
}

$formatter = $null
$detected = @()
foreach ($candidate in $candidates) {
    $commands = @(Get-Command $candidate -All -CommandType Application, ExternalScript -ErrorAction SilentlyContinue)
    foreach ($command in $commands) {
        $versionOutput = (& $command.Source --version 2>&1 | Out-String).Trim()
        if ($LASTEXITCODE -eq 0 -and $versionOutput -match '\bclang-format version (\d+\.\d+\.\d+)\b' -and $Matches[1] -eq $expectedVersion) {
            $formatter = $command.Source
            break
        }
        $detected += "$($command.Source): $versionOutput"
    }
    if ($null -ne $formatter) { break }
}

if ($null -eq $formatter) {
    $details = if ($detected.Count) { " Detected: " + ($detected -join "; ") } else { "" }
    throw "clang-format $expectedVersion is required.$details Install: python -m pip install clang-format==$expectedVersion. Use -ClangFormat or CLANG_FORMAT to select its executable."
}

$cppFiles = @(
    Get-ChildItem -LiteralPath (Join-Path $projectRoot "src"), (Join-Path $projectRoot "tests") -Recurse -File |
        Where-Object { $_.Extension -in ".cpp", ".h" } |
        Sort-Object FullName |
        ForEach-Object FullName
) + @((Join-Path $projectRoot "main.cpp"))
if ($cppFiles.Count -eq 0) {
    throw "No C++ files found in $projectRoot"
}

[string[]]$formatArguments = if ($Check) { @("--dry-run", "--Werror") } else { @("-i") }
Write-Host "clang-format $expectedVersion ($formatter): $($cppFiles.Count) C++ files"
# The pipeline also waits for Windows executables built with a GUI subsystem.
& $formatter @formatArguments @cppFiles 2>&1 | Write-Output
if ($LASTEXITCODE -ne 0) {
    throw "C++ formatting failed with exit code $LASTEXITCODE. Run scripts/format-cpp.ps1 to apply the pinned formatter."
}
