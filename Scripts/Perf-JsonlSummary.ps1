#Requires -Version 5.1
<#
.SYNOPSIS
  Print p50 frameMs from --perf-log JSONL (schemaVersion 1).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Path
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Path)) {
    throw "Perf log not found: $Path"
}

$frameMs = [System.Collections.Generic.List[double]]::new()
Get-Content -Path $Path -Encoding UTF8 | ForEach-Object {
    $line = $_.Trim()
    if ([string]::IsNullOrWhiteSpace($line)) { return }
    $obj = $line | ConvertFrom-Json
    if ($null -ne $obj.frameMs) {
        [void]$frameMs.Add([double]$obj.frameMs)
    }
}

if ($frameMs.Count -eq 0) {
    throw "No frameMs entries in $Path"
}

$sorted = $frameMs | Sort-Object
$mid = [int][math]::Floor(($sorted.Count - 1) * 0.5)
$p50 = $sorted[$mid]

Write-Host "frames=$($sorted.Count) p50_frameMs=$p50"
exit 0
