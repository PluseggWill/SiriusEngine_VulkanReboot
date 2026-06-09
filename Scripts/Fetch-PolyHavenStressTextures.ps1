#Requires -Version 5.1
<#
.SYNOPSIS
  Download CC0 Poly Haven 1K diffuse JPGs for stress scene materials.
.DESCRIPTION
  Idempotent: skips files already present. Source: Poly Haven API (https://polyhaven.com).
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

function Get-PolyHavenDiffuseJpg1kUrl {
    param( [string]$AssetId )
    $files = Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$AssetId"
    return $files.Diffuse.'1k'.jpg.url
}

# Repo filename -> Poly Haven texture id
$textureMap = @{
    "ph_grass_ground_diff_1k.jpg" = "sparse_grass"
    "ph_foliage_diff_1k.jpg"       = "forest_leaves_03"
    "ph_cliff_rock_diff_1k.jpg"    = "cliff_side"
    "ph_rock_face_diff_1k.jpg"     = "rock_face"
    "ph_grass_path_diff_1k.jpg"    = "grass_path_3"
}

$destDir = Join-Path $RepoRoot "Data\Textures"
if ( -not ( Test-Path $destDir ) ) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

$copied  = 0
$skipped = 0
foreach ( $entry in $textureMap.GetEnumerator() ) {
    $dest = Join-Path $destDir $entry.Key
    if ( Test-Path $dest ) {
        $skipped++
        continue
    }
    $url = Get-PolyHavenDiffuseJpg1kUrl -AssetId $entry.Value
    Write-Host "[FETCH] $($entry.Key) <- $($entry.Value)"
    Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
    $copied++
}

Write-Host "[FETCH] Done. copied=$copied skipped=$skipped dest=$destDir"
