#Requires -Version 5.1
<#
.SYNOPSIS
  Regenerate shaders via CompileShader_Glslc.bat and fail if committed outputs drift.
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepoRoot = (Resolve-Path $RepoRoot).Path
}

$bat = Join-Path $RepoRoot "VulkanDesktop\Shader\CompileShader_Glslc.bat"
if (-not (Test-Path $bat)) {
    throw "Shader compile script not found: $bat"
}

Write-Host "Running CompileShader_Glslc.bat ..."
cmd /c "call `"$bat`""
if ($LASTEXITCODE -ne 0) {
    throw "CompileShader_Glslc.bat failed with exit $LASTEXITCODE"
}

$gen = Join-Path $RepoRoot "VulkanDesktop\Shader_Generated"
# reflection_*.json embeds absolute .spv paths from the host machine — not drift-checked (contracts + .spv are).
$patterns = @(
    "*.spv",
    "DescriptorContract_LitBatch.json",
    "DescriptorContract_LitBindless.json"
)

$paths = @()
foreach ($pat in $patterns) {
    $paths += Get-ChildItem -Path $gen -Filter $pat -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
}

if ($paths.Count -eq 0) {
    throw "No shader generated artifacts under $gen"
}

Push-Location $RepoRoot
try {
    & git diff --exit-code -- $paths
    $exit = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($exit -ne 0) {
    Write-Host "Shader drift detected. Regenerate locally and commit, or fix GLSL:" -ForegroundColor Red
    & git diff --stat -- $paths
    exit 1
}

Write-Host "Shader outputs match committed tree." -ForegroundColor Green
exit 0
