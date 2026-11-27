$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$formatScript = Join-Path $projectRoot "scripts/format-cpp.ps1"
$version = (Get-Content (Join-Path (Split-Path $projectRoot -Parent) ".clang-format-version") -Raw).Trim()
$fixture = Join-Path $projectRoot ("build/formatter-tests/" + [guid]::NewGuid())
New-Item -ItemType Directory -Force $fixture | Out-Null

$mock = @'
$ToolArguments = @($args)
if ($ToolArguments -contains "--version") {
    Write-Output "clang-format version EXPECTED_VERSION"
    exit 0
}
$ToolArguments | Set-Content -LiteralPath (Join-Path $PSScriptRoot "arguments.txt")
exit 0
'@
$matching = Join-Path $fixture "matching.ps1"
$wrong = Join-Path $fixture "wrong.ps1"
$failing = Join-Path $fixture "failing.ps1"
$mock.Replace("EXPECTED_VERSION", $version) | Set-Content -LiteralPath $matching
$mock.Replace("EXPECTED_VERSION", "99.0.0") | Set-Content -LiteralPath $wrong
($mock.Replace("EXPECTED_VERSION", $version) -replace 'exit 0\s*$', 'exit 47') | Set-Content -LiteralPath $failing

try {
    & $formatScript -Check -ClangFormat $wrong
    throw "Wrong formatter version was accepted"
} catch {
    if ($_.Exception.Message -notmatch 'is required') { throw }
}
if (Test-Path (Join-Path $fixture "arguments.txt")) {
    throw "Wrong formatter was allowed to process source files"
}

& $formatScript -Check -ClangFormat $matching
$captured = @(Get-Content (Join-Path $fixture "arguments.txt"))
foreach ($required in @("--dry-run", "--Werror", (Join-Path $projectRoot "main.cpp"),
    (Join-Path $projectRoot "src/Data/Exercises/Exercises_Repository.h"),
    (Join-Path $projectRoot "tests/cpp/tst_Measurement_Schedule.cpp"))) {
    if ($captured -notcontains $required) { throw "Missing formatter argument: $required" }
}
if ($captured | Where-Object { $_ -match '[\\/]build[\\/]' }) {
    throw "Build artifacts must not be formatted"
}

& $formatScript -ClangFormat $matching
$captured = @(Get-Content (Join-Path $fixture "arguments.txt"))
if ($captured -notcontains "-i" -or $captured -contains "--dry-run") {
    throw "Apply mode did not request in-place formatting"
}

try {
    & $formatScript -Check -ClangFormat $failing
    throw "Formatter failure was ignored"
} catch {
    if ($_.Exception.Message -notmatch 'C\+\+ formatting failed with exit code 47') { throw }
}

$previousFormatter = $env:CLANG_FORMAT
try {
    $env:CLANG_FORMAT = $matching
    & $formatScript -Check
} finally {
    $env:CLANG_FORMAT = $previousFormatter
}

$previousPath = $env:PATH
$previousFormatter = $env:CLANG_FORMAT
try {
    $first = Join-Path $fixture "first"
    $second = Join-Path $fixture "second"
    New-Item -ItemType Directory -Force $first, $second | Out-Null
    if ([IO.Path]::PathSeparator -eq ';') {
        $mock.Replace("EXPECTED_VERSION", "99.0.0") | Set-Content (Join-Path $first "clang-format.ps1")
        $mock.Replace("EXPECTED_VERSION", $version) | Set-Content (Join-Path $second "clang-format.ps1")
    } else {
        $shellMock = @'
#!/bin/sh
if [ "$1" = "--version" ]; then
    printf '%s\n' 'clang-format version EXPECTED_VERSION'
    exit 0
fi
printf '%s\n' "$@" > "$(dirname "$0")/arguments.txt"
'@
        $firstTool = Join-Path $first "clang-format"
        $secondTool = Join-Path $second "clang-format"
        $shellMock.Replace("EXPECTED_VERSION", "99.0.0") | Set-Content -LiteralPath $firstTool -Encoding utf8
        $shellMock.Replace("EXPECTED_VERSION", $version) | Set-Content -LiteralPath $secondTool -Encoding utf8
        & chmod +x $firstTool $secondTool
        if ($LASTEXITCODE -ne 0) { throw "Could not prepare PATH test fixtures" }
    }
    $env:CLANG_FORMAT = $null
    $env:PATH = $first + [IO.Path]::PathSeparator + $second + [IO.Path]::PathSeparator + $previousPath
    & $formatScript -Check
    if (-not (Test-Path (Join-Path $second "arguments.txt")) -or (Test-Path (Join-Path $first "arguments.txt"))) {
        throw "PATH discovery did not select the matching formatter version"
    }
} finally {
    $env:PATH = $previousPath
    $env:CLANG_FORMAT = $previousFormatter
}

Write-Host "Formatter guard tests passed: version rejection, check/apply modes, source discovery, failures, environment override, multiple PATH versions."
exit 0
