#Requires -Version 5.1
<#
.SYNOPSIS
  Generate offline IBL assets for Data/Environments/default (S5).
.DESCRIPTION
  Downloads a CC0 Poly Haven HDRI and bakes sky / irradiance / prefilter / BRDF LUT
  via Scripts/bake_default_ibl.py. Requires Python + numpy.
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$HdriSlug = "kloppenheim_02",
    [string]$HdriResolution = "1k"
)

$ErrorActionPreference = "Stop"

$python = Get-Command python -ErrorAction SilentlyContinue
if ( -not $python ) {
    throw "Python is required to bake IBL assets (Scripts/bake_default_ibl.py)."
}

& $python.Source (Join-Path $PSScriptRoot "bake_default_ibl.py") `
    --repo-root $RepoRoot `
    --hdri-slug $HdriSlug `
    --hdri-resolution $HdriResolution
if ( $LASTEXITCODE -ne 0 ) {
    throw "bake_default_ibl.py failed with exit code $LASTEXITCODE"
}

Write-Host "IBL assets baked under $(Join-Path $RepoRoot 'Data\Environments\default') from Poly Haven HDRI '$HdriSlug'."
