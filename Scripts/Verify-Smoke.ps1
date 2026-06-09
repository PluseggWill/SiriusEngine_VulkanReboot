#Requires -Version 5.1
<#
.SYNOPSIS
  G0-smoke: graceful VulkanDesktop smoke + Assert-SmokeLog.ps1

.EXAMPLE
  powershell -File Scripts/Verify-Smoke.ps1
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug",
    [int] $SmokeFrames = 120,
    [double] $SmokeSeconds = 6
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepoRoot = (Resolve-Path $RepoRoot).Path
}

$exe = Join-Path $RepoRoot "x64\$Configuration\VulkanDesktop.exe"
if (-not (Test-Path $exe)) {
    throw "Missing $exe - build Debug|x64 first (run Scripts/Verify-CI.ps1)"
}

Write-Host "=== Verify-Smoke: VulkanDesktop ===" -ForegroundColor Cyan
& $exe `
    --asset-root $RepoRoot `
    --config (Join-Path $RepoRoot "Config\engine.benchmark.json") `
    --scene Data/Scenes/smoke.json `
    --no-validation `
    --smoke-frames $SmokeFrames `
    --smoke-seconds $SmokeSeconds

$exitCode = $LASTEXITCODE
& (Join-Path $PSScriptRoot "Assert-SmokeLog.ps1") -RepoRoot $RepoRoot -ExitCode $exitCode

if ($exitCode -ne 0) {
    throw "VulkanDesktop smoke exit $exitCode"
}

Write-Host "=== Verify-Smoke: PASSED ===" -ForegroundColor Green
exit 0
