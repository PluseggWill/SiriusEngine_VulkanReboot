#Requires -Version 5.1
<#
.SYNOPSIS
  Assert graceful smoke log tokens and optional assetRoot under repo.
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [string] $LogPath = "",
    [int] $ExitCode = 0
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepoRoot = (Resolve-Path $RepoRoot).Path
}

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $LogPath = Join-Path $RepoRoot "Logs\engine_runtime_log.txt"
}

if ($ExitCode -ne 0) {
    throw "VulkanDesktop exit code was $ExitCode (expected 0)"
}

if (-not (Test-Path $LogPath)) {
    throw "Runtime log missing: $LogPath"
}

$logText = Get-Content -Path $LogPath -Raw -Encoding UTF8
$required = @(
    '[CONFIG] assetRoot=',
    '[SCENE] LoadSceneResources completed',
    '[APP] Smoke dwell reached',
    '[SCENE] UnloadScene',
    '[APP] Engine exited run loop normally'
)

$missing = @()
foreach ($token in $required) {
    if ($logText -notmatch [regex]::Escape($token)) {
        $missing += $token
    }
}

if ($missing.Count -gt 0) {
    throw "Smoke log missing tokens: $($missing -join ', ') ($LogPath)"
}

if ($logText -match '\[CONFIG\] assetRoot=(.+)') {
    $loggedRoot = $Matches[1].Trim()
    $canonicalRepo = (Resolve-Path $RepoRoot).Path
    $canonicalLogged = $null
    try {
        $canonicalLogged = (Resolve-Path $loggedRoot -ErrorAction Stop).Path
    } catch {
        throw "assetRoot in log is not a valid path: $loggedRoot"
    }
    if (-not $canonicalLogged.StartsWith($canonicalRepo, [StringComparison]::OrdinalIgnoreCase)) {
        throw "assetRoot outside repo: $canonicalLogged (expected under $canonicalRepo)"
    }
}

Write-Host "Smoke log OK: $LogPath" -ForegroundColor Green
exit 0
