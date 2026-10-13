[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repositoryRoot = (& git -C $projectRoot rev-parse --show-toplevel).Trim()
if ($LASTEXITCODE -ne 0 -or -not $repositoryRoot) {
    throw "TrainingAppClient is not inside a Git repository."
}

& git -C $repositoryRoot config core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw "Could not configure core.hooksPath."
}

$isWindowsPlatform = $env:OS -eq "Windows_NT"
if (-not $isWindowsPlatform) {
    & chmod +x (Join-Path $repositoryRoot ".githooks/pre-commit")
    if ($LASTEXITCODE -ne 0) {
        throw "Could not make the pre-commit hook executable."
    }
}

Write-Host "Git hooks enabled for this clone: $repositoryRoot/.githooks" -ForegroundColor Green
Write-Host "No commit or push was performed."