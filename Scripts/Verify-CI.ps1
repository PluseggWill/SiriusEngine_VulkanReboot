#Requires -Version 5.1
<#
.SYNOPSIS
  G0 CI entry: clang-format, MSBuild Debug|x64, shader drift, GfxTests.

.EXAMPLE
  powershell -File Scripts/Verify-CI.ps1
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug",
    [ValidateSet("x64")]
    [string] $Platform = "x64",
    [switch] $SkipShaderDrift,
    [switch] $SkipClangFormat
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepoRoot = (Resolve-Path $RepoRoot).Path
}

if (-not (Test-Path (Join-Path $RepoRoot "VulkanDesktop.sln"))) {
    throw "VulkanDesktop.sln not found under $RepoRoot"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio 2022 with C++ workload."
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($msbuild) -or -not (Test-Path $msbuild)) {
    throw "MSBuild not found via vswhere."
}

if (-not $SkipClangFormat) {
    Write-Host "=== Verify-CI: clang-format ===" -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "Assert-ClangFormat.ps1") -RepoRoot $RepoRoot -BootstrapTools
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$sln = Join-Path $RepoRoot "VulkanDesktop.sln"
Write-Host "=== Verify-CI: MSBuild $Configuration|$Platform ===" -ForegroundColor Cyan
& $msbuild $sln /p:Configuration=$Configuration /p:Platform=$Platform /v:m /nologo
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit $LASTEXITCODE"
}

if (-not $SkipShaderDrift) {
    Write-Host "=== Verify-CI: shader drift ===" -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "Assert-ShaderDrift.ps1") -RepoRoot $RepoRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$gfxTests = Join-Path $RepoRoot "x64\$Configuration\GfxTests.exe"
if (-not (Test-Path $gfxTests)) {
    throw "GfxTests.exe missing: $gfxTests (build GfxTests project)"
}

Write-Host "=== Verify-CI: GfxTests ===" -ForegroundColor Cyan
$gfxProc = Start-Process -FilePath $gfxTests -WorkingDirectory $RepoRoot -Wait -PassThru -NoNewWindow
if ($gfxProc.ExitCode -ne 0) {
    throw "GfxTests failed with exit $($gfxProc.ExitCode)"
}

Write-Host "=== Verify-CI: PASSED ===" -ForegroundColor Green
exit 0
