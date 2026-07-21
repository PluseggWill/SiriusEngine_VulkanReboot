#Requires -Version 5.1
<#
.SYNOPSIS
  G0-smoke: graceful VulkanDesktop smoke + Assert-SmokeLog.ps1

  Pass 1 — CPU indirect (default path) on sponza (matches Config/engine.json).
  Pass 2 — --gpu-cull dogfood on same scene (M2 acceptance); skip with -SkipGpuCull.

.EXAMPLE
  powershell -File Scripts/Verify-Smoke.ps1
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug",
    [int] $SmokeFrames = 120,
    [double] $SmokeSeconds = 6,
    [switch] $SkipGpuCull
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

$defaultConfig = Join-Path $RepoRoot "Config\engine.json"
$commonArgs = @(
    "--asset-root", $RepoRoot,
    "--config", $defaultConfig,
    "--scene", "Data/Scenes/sponza.json",
    "--no-validation",
    "--smoke-frames", $SmokeFrames,
    "--smoke-seconds", $SmokeSeconds
)

function Invoke-SmokePass {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Label,
        [ValidateSet("Default", "GpuCull")]
        [string] $Profile,
        [string[]] $ExtraArgs = @()
    )

    Write-Host "=== Verify-Smoke: $Label ===" -ForegroundColor Cyan
    & $exe @commonArgs @ExtraArgs
    $exitCode = $LASTEXITCODE
    & (Join-Path $PSScriptRoot "Assert-SmokeLog.ps1") -RepoRoot $RepoRoot -ExitCode $exitCode -Profile $Profile
    if ($exitCode -ne 0) {
        throw "VulkanDesktop smoke exit $exitCode ($Label)"
    }
}

Invoke-SmokePass -Label "CPU indirect (default)" -Profile Default

if (-not $SkipGpuCull) {
    Invoke-SmokePass -Label "GPU cull dogfood (--gpu-cull)" -Profile GpuCull -ExtraArgs @("--gpu-cull")
}

Write-Host "=== Verify-Smoke: PASSED ===" -ForegroundColor Green
exit 0
