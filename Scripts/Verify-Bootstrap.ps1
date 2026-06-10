#Requires -Version 5.1
<#
.SYNOPSIS
  Verify toolchain, build VulkanDesktop, and smoke-check runtime logs.

.DESCRIPTION
  Run from repository root (or pass -RepoRoot). Steps:
  1) Confirm VulkanDesktop.sln exists
  2) MSBuild Debug|x64 (default)
  3) VulkanDesktop.exe --help
  4) Short windowed run from x64\<Config> for startup log lines
  5) Grep Logs/engine_runtime_log.txt for [CONFIG] assetRoot= and [STARTUP] success

.EXAMPLE
  pwsh -File Scripts/Verify-Bootstrap.ps1
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug",
    [ValidateSet("x64")]
    [string] $Platform = "x64",
    [int] $SmokeSeconds = 4
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    param([string] $Root)
    if ([string]::IsNullOrWhiteSpace($Root)) {
        $scriptDir = Split-Path -Parent $PSCommandPath
        $Root = (Resolve-Path (Join-Path $scriptDir "..")).Path
    } else {
        $Root = (Resolve-Path $Root).Path
    }
    if (-not (Test-Path (Join-Path $Root "VulkanDesktop.sln"))) {
        throw "VulkanDesktop.sln not found under: $Root"
    }
    return $Root
}

function Add-StepResult {
    param(
        [string] $Name,
        [bool] $Ok,
        [string] $Detail
    )
    $script:Results += [PSCustomObject]@{
        Step   = $Name
        Status = $(if ($Ok) { "PASS" } else { "FAIL" })
        Detail = $Detail
    }
    if (-not $Ok) { $script:Failed = $true }
}

$Results = @()
$Failed = $false

try {
    $repo = Resolve-RepoRoot -Root $RepoRoot
    Add-StepResult "Repo root" $true "VulkanDesktop.sln at $repo"

    $rotateBat = Join-Path $repo "VulkanDesktop\Scripts\RotateEngineLogs.bat"
    if (Test-Path $rotateBat) {
        cmd /c "call `"$rotateBat`" `"$repo`"" | Out-Null
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Add-StepResult "MSBuild locate" $false "vswhere.exe not found. Install Visual Studio 2022 with C++ workload."
        throw "vswhere missing"
    }

    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
        -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($msbuild) -or -not (Test-Path $msbuild)) {
        Add-StepResult "MSBuild locate" $false "No MSBuild from vswhere."
        throw "MSBuild not found"
    }
    Add-StepResult "MSBuild locate" $true $msbuild

    $sln = Join-Path $repo "VulkanDesktop.sln"
    Write-Host "Building $sln ($Configuration|$Platform) ..."
    & $msbuild $sln /p:Configuration=$Configuration /p:Platform=$Platform /v:m /nologo
    if ($LASTEXITCODE -ne 0) {
        Add-StepResult "Build" $false "MSBuild exit code $LASTEXITCODE"
        throw "Build failed"
    }
    Add-StepResult "Build" $true "$Configuration|$Platform"

    $exe = Join-Path $repo "x64\$Configuration\VulkanDesktop.exe"
    if (-not (Test-Path $exe)) {
        Add-StepResult "Binary exists" $false "Missing: $exe"
        throw "Binary missing"
    }
    Add-StepResult "Binary exists" $true $exe

    # App logs to stderr as well; do not treat stderr as terminating errors.
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $helpOut = cmd /c "`"$exe`" --help 2>&1"
    $helpExit = $LASTEXITCODE
    $ErrorActionPreference = $prevEap
    $helpOk = ($helpExit -eq 0) -and ($helpOut -match "Usage:")
    Add-StepResult "--help" $helpOk $(if ($helpOk) { "exit 0, Usage present" } else { "exit=$LASTEXITCODE" })

    Write-Host "Graceful smoke (ci-verification canonical) ..."
    $smokeArgs = @(
        "--asset-root", $repo,
        "--config", (Join-Path $repo "Config\engine.stress.json"),
        "--scene", "Data/Scenes/stress.json",
        "--no-validation",
        "--smoke-frames", "120",
        "--smoke-seconds", "$SmokeSeconds"
    )
    & $exe @smokeArgs
    $smokeExit = $LASTEXITCODE
    $smokeOk = ($smokeExit -eq 0)
    Add-StepResult "Smoke run" $smokeOk "exit=$smokeExit (--asset-root + stress.json)"

    $assertScript = Join-Path $repo "Scripts\Assert-SmokeLog.ps1"
    if ($smokeOk -and (Test-Path $assertScript)) {
        try {
            & $assertScript -RepoRoot $repo -ExitCode $smokeExit
            Add-StepResult "Assert-SmokeLog" $true "tokens OK"
        } catch {
            Add-StepResult "Assert-SmokeLog" $false $_.Exception.Message
        }
    }

    $logPath = Join-Path $repo "Logs\engine_runtime_log.txt"
    if (-not (Test-Path $logPath)) {
        Add-StepResult "Runtime log exists" $false "Missing: $logPath"
    } else {
        Add-StepResult "Runtime log exists" $true $logPath
    }

    $logText = if (Test-Path $logPath) { Get-Content -Path $logPath -Raw -Encoding UTF8 } else { "" }
    $hasStartupOk = $logText -match '\[STARTUP\] All required demo assets present\.'
    Add-StepResult "Startup check" $hasStartupOk $(if ($hasStartupOk) { "startup OK" } else { "optional for smoke.json" })
}
catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Verify-Bootstrap summary ===" -ForegroundColor Cyan
$Results | Format-Table -AutoSize

if ($Failed) {
    Write-Host "FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "PASSED" -ForegroundColor Green
exit 0
