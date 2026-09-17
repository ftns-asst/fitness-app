[CmdletBinding()]
param(
    [string]$BuildDir = "build/ci",
    [string]$Generator = "Ninja",
    [string]$QtPrefix = "",
    [switch]$SkipFormat
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $projectRoot $BuildDir
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    Write-Host "> $Command $($Arguments -join ' ')" -ForegroundColor Cyan
    $previousErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Command @Arguments
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorPreference
    }
    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}: $Command"
    }
}

function Resolve-Tool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [string[]]$Candidates = @()
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Required tool was not found: $Name"
    }
    return $command.Source
}

function Add-PathDirectory {
    param(
        [string]$Directory
    )

    if ($Directory -and (Test-Path $Directory)) {
        $pathEntries = $env:PATH -split [System.IO.Path]::PathSeparator
        if ($pathEntries -notcontains $Directory) {
            $env:PATH = $Directory + [System.IO.Path]::PathSeparator + $env:PATH
        }
    }
}

function Test-QmlFormat {
    param(
        [Parameter(Mandatory = $true)]
        [string]$QmlFormat
    )

    $temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("training-app-qmlformat-" + [guid]::NewGuid())
    $qmlFiles = Get-ChildItem (Join-Path $projectRoot "qml") -Recurse -Filter "*.qml" -File
    $qmlFiles += Get-ChildItem (Join-Path $projectRoot "tests") -Recurse -Filter "*.qml" -File

    New-Item $temporaryRoot -ItemType Directory | Out-Null
    try {
        foreach ($file in $qmlFiles) {
            $relativePath = $file.FullName.Substring($projectRoot.Length).TrimStart("\", "/")
            $temporaryFile = Join-Path $temporaryRoot $relativePath
            $temporaryDirectory = Split-Path $temporaryFile -Parent

            New-Item $temporaryDirectory -ItemType Directory -Force | Out-Null
            Copy-Item $file.FullName $temporaryFile
            Invoke-Checked $QmlFormat "-i" $temporaryFile

            $sourceHash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash
            $formattedHash = (Get-FileHash $temporaryFile -Algorithm SHA256).Hash
            if ($sourceHash -ne $formattedHash) {
                throw "QML file is not formatted: $relativePath. Run qmlformat -i on it."
            }
        }
        Write-Host "QML format check passed ($($qmlFiles.Count) files)." -ForegroundColor Green
    } finally {
        Remove-Item $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$isWindowsPlatform = $env:OS -eq "Windows_NT"
if (-not $QtPrefix -and $env:QT_ROOT_DIR) {
    $QtPrefix = $env:QT_ROOT_DIR
}
if (-not $QtPrefix -and $isWindowsPlatform) {
    foreach ($candidate in @("D:/Qt/6.11.2/mingw_64", "C:/Qt/6.11.2/mingw_64")) {
        if (Test-Path (Join-Path $candidate "bin/qmlformat.exe")) {
            $QtPrefix = $candidate
            break
        }
    }
}

$qtBin = if ($QtPrefix) { Join-Path $QtPrefix "bin" } else { "" }
if ($isWindowsPlatform) {
    Add-PathDirectory $qtBin
    Add-PathDirectory "D:/Qt/Tools/CMake_64/bin"
    Add-PathDirectory "D:/Qt/Tools/Ninja"
    Add-PathDirectory "D:/Qt/Tools/mingw1310_64/bin"
    Add-PathDirectory "C:/Qt/Tools/CMake_64/bin"
    Add-PathDirectory "C:/Qt/Tools/Ninja"
    Add-PathDirectory "C:/Qt/Tools/mingw1310_64/bin"
}

$cmake = Resolve-Tool "cmake" @(
    "D:/Qt/Tools/CMake_64/bin/cmake.exe",
    "C:/Qt/Tools/CMake_64/bin/cmake.exe"
)
$ctest = Resolve-Tool "ctest" @(
    $(if ($cmake) { Join-Path (Split-Path $cmake -Parent) "ctest.exe" } else { "" }),
    $(if ($cmake) { Join-Path (Split-Path $cmake -Parent) "ctest" } else { "" })
)
$qmlformat = Resolve-Tool "qmlformat" @(
    $(if ($qtBin) { Join-Path $qtBin "qmlformat.exe" } else { "" }),
    $(if ($qtBin) { Join-Path $qtBin "qmlformat" } else { "" })
)
$clangFormat = Resolve-Tool "clang-format" @(
    "D:/Qt/Tools/QtCreator/bin/clang/bin/clang-format.exe"
)

if (-not $SkipFormat) {
    Test-QmlFormat $qmlformat

    $cppFiles = Get-ChildItem $projectRoot -Recurse -Include "*.cpp", "*.h" -File |
        Where-Object { $_.FullName -notmatch "[\\/]build[\\/]" -and $_.FullName -notmatch "[\\/]importedcontent[\\/]" } |
        ForEach-Object { $_.FullName }
    Invoke-Checked $clangFormat "--dry-run" "--Werror" @cppFiles
}

$configureArguments = @(
    "-S", $projectRoot,
    "-B", $resolvedBuildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DBUILD_TESTING=ON",
    "-DTRAINING_APP_GENERATE_QMLLS_INI=OFF"
)
if ($QtPrefix) {
    $configureArguments += "-DCMAKE_PREFIX_PATH=$QtPrefix"
}

Invoke-Checked $cmake @configureArguments
Invoke-Checked $cmake "--build" $resolvedBuildDir "--target" "appTrainingAppClient_qmllint" "appTrainingAppClient" "--parallel"

$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_FORCE_STDERR_LOGGING = "1"
Invoke-Checked $ctest "--test-dir" $resolvedBuildDir "--output-on-failure" "--timeout" "60"

Write-Host "All TrainingAppClient gates passed." -ForegroundColor Green