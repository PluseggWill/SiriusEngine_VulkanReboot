#Requires -Version 5.1
<#
.SYNOPSIS
  Optional WSI resize soak: launch demo scene, resize GLFW window, assert swapchain recreate in log.

  CI: soft-fail optional job — Win32 resize is best-effort; manual gate documented in Docs/CLI.md.

.EXAMPLE
  powershell -File Scripts/Verify-ResizeSmoke.ps1
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Debug",
    [double] $SmokeSeconds = 8
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

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32Resize {
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    public const uint SWP_NOMOVE = 0x0002;
    public const uint SWP_NOZORDER = 0x0004;
}
"@

Write-Host "=== Verify-ResizeSmoke: launch + resize ===" -ForegroundColor Cyan

$proc = Start-Process -FilePath $exe -PassThru -WorkingDirectory (Split-Path $exe) -ArgumentList @(
    "--asset-root", $RepoRoot,
    "--no-validation",
    "--scene", "Data/Scenes/demo.json",
    "--smoke-seconds", [string]$SmokeSeconds
)

Start-Sleep -Seconds 2

$windowTitle = "Vulkan Window"
$hwnd = [Win32Resize]::FindWindow([NullString]::Value, $windowTitle)
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Warning "Could not find window '$windowTitle'; continuing without programmatic resize (manual gate)."
} else {
    $sizes = @(@(1280, 720), @(960, 540), @(1400, 900))
    foreach ($pair in $sizes) {
        [void][Win32Resize]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, $pair[0], $pair[1], [Win32Resize]::SWP_NOMOVE -bor [Win32Resize]::SWP_NOZORDER)
        Start-Sleep -Milliseconds 400
    }
}

if (-not $proc.HasExited) {
    Wait-Process -Id $proc.Id
}
$exitCode = $proc.ExitCode

$logPath = Join-Path $RepoRoot "Logs\engine_runtime_log.txt"
if (-not (Test-Path $logPath)) {
    throw "Missing log: $logPath"
}

$logText = Get-Content -Raw -Path $logPath
if ($logText -notmatch "\[SWAPCHAIN\].*rebuild layer=wsi") {
    throw "Log missing [SWAPCHAIN] rebuild layer=wsi (resize may not have triggered recreate)."
}
if ($logText -match "\[ERROR\]") {
    throw "Log contains [ERROR] during resize smoke."
}

& (Join-Path $PSScriptRoot "Assert-SmokeLog.ps1") -RepoRoot $RepoRoot -ExitCode $exitCode

if ($exitCode -ne 0) {
    throw "VulkanDesktop resize smoke exit $exitCode"
}

Write-Host "=== Verify-ResizeSmoke: PASSED ===" -ForegroundColor Green
exit 0
