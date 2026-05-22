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

    $runDir = Join-Path $repo "x64\$Configuration"
    Write-Host "Smoke run ($SmokeSeconds s) from $runDir ..."
    Push-Location $runDir
    try {
        $proc = Start-Process -FilePath $exe -PassThru -WindowStyle Minimized
        Start-Sleep -Seconds $SmokeSeconds
        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }
        $smokeOk = $true
        Add-StepResult "Smoke run" $smokeOk "Windowed run ${SmokeSeconds}s (stopped if alive)"
    } finally {
        Pop-Location
    }

    $logPath = Join-Path $repo "Logs\engine_runtime_log.txt"
    if (-not (Test-Path $logPath)) {
        Add-StepResult "Runtime log" $false "Missing: $logPath"
        throw "Log missing"
    }

    $logText = Get-Content -Path $logPath -Raw -Encoding UTF8
    $hasAssetRoot = $logText -match '\[CONFIG\] assetRoot='
    $hasStartupOk = $logText -match '\[STARTUP\] All required demo assets present\.'
    $logOk = $hasAssetRoot -and $hasStartupOk
    $detail = @(
        $(if ($hasAssetRoot) { "assetRoot= found" } else { "assetRoot= missing" }),
        $(if ($hasStartupOk) { "startup OK found" } else { "startup OK missing" })
    ) -join "; "
    Add-StepResult "Log keywords" $logOk "$detail ($logPath)"
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
