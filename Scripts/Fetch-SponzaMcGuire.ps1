#Requires -Version 5.1
<#
.SYNOPSIS
  Download Crytek Sponza (McGuire archive) into Data/Models/sponza/source/.
.DESCRIPTION
  Idempotent: skips download when source/sponza.obj and source/sponza.mtl both exist.
  Source: Morgan McGuire Computer Graphics Archive (CC BY 3.0, Crytek / Frank Meinl).
  After fetch, runs Generate-SponzaScene.ps1 unless -SkipGenerate is set.
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [switch]$SkipGenerate
)

$ErrorActionPreference = "Stop"

$zipUrl      = "https://casual-effects.com/g3d/data10/common/model/crytek_sponza/sponza.zip"
$zipPath     = Join-Path $env:TEMP "mcguire_sponza.zip"
$sourceDir   = Join-Path $RepoRoot "Data\Models\sponza\source"
$markerObj   = Join-Path $sourceDir "sponza.obj"
$markerMtl   = Join-Path $sourceDir "sponza.mtl"
$sourceReady = ( Test-Path $markerObj ) -and ( Test-Path $markerMtl )

if ( $sourceReady ) {
    Write-Host "[FETCH] Sponza source already present: $markerObj"
}
else {
    if ( ( Test-Path $zipPath ) -and -not $sourceReady ) {
        Write-Host "[FETCH] Removing stale zip cache (source missing)."
        Remove-Item -Force $zipPath
    }
    if ( -not ( Test-Path $zipPath ) ) {
        Write-Host "[FETCH] Downloading McGuire Sponza (~80 MB)..."
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
    }

    $extractTemp = Join-Path $env:TEMP "mcguire_sponza_extract"
    if ( Test-Path $extractTemp ) {
        Remove-Item -Recurse -Force $extractTemp
    }
    Write-Host "[FETCH] Extracting to $extractTemp"
    Expand-Archive -Path $zipPath -DestinationPath $extractTemp -Force

    if ( -not ( Test-Path ( Join-Path $extractTemp "sponza.obj" ) ) ) {
        throw "Unexpected Sponza zip layout: missing sponza.obj"
    }

    if ( Test-Path $sourceDir ) {
        Remove-Item -Recurse -Force $sourceDir
    }
    New-Item -ItemType Directory -Path $sourceDir -Force | Out-Null
    Copy-Item -Path ( Join-Path $extractTemp "*" ) -Destination $sourceDir -Recurse -Force
    Write-Host "[FETCH] Installed source assets to $sourceDir"
}

if ( -not $SkipGenerate ) {
    $genScript = Join-Path $PSScriptRoot "Generate-SponzaScene.ps1"
    & $genScript -RepoRoot $RepoRoot
}
