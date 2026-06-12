#Requires -Version 5.1
<#
.SYNOPSIS
  Generate offline IBL assets for Data/Environments/default (S5 v0).
.DESCRIPTION
  Idempotent: skips existing PNGs. Produces cubemap face PNGs + split-sum BRDF LUT.
  No network. Run from repo root before first S5 smoke with IBL enabled.
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$envRoot = Join-Path $RepoRoot "Data\Environments\default"

function Save-Png {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [string]$Path
    )
    $dir = Split-Path $Path -Parent
    if ( -not ( Test-Path $dir ) ) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $Bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
}

function New-SolidFace {
    param(
        [int]$Size,
        [byte]$R,
        [byte]$G,
        [byte]$B
    )
    $bmp = New-Object System.Drawing.Bitmap $Size, $Size
    $color = [System.Drawing.Color]::FromArgb(255, $R, $G, $B)
    for ( $y = 0; $y -lt $Size; $y++ ) {
        for ( $x = 0; $x -lt $Size; $x++ ) {
            $bmp.SetPixel($x, $y, $color)
        }
    }
    return $bmp
}

# Soft outdoor palette — enough variation for pipeline debug, not final art.
$faceColors = @{
    posx = @(140, 175, 210)
    negx = @(130, 165, 200)
    posy = @(185, 215, 245)
    negy = @(55, 60, 70)
    posz = @(150, 185, 220)
    negz = @(135, 170, 205)
}

function Write-CubemapFaces {
    param(
        [string]$SubDir,
        [int]$Size
    )
    $outDir = Join-Path $envRoot $SubDir
    foreach ( $face in $faceColors.Keys ) {
        $path = Join-Path $outDir "$face.png"
        if ( Test-Path $path ) { continue }
        $rgb  = $faceColors[$face]
        $bmp  = New-SolidFace -Size $Size -R $rgb[0] -G $rgb[1] -B $rgb[2]
        Save-Png -Bitmap $bmp -Path $path
        $bmp.Dispose()
    }
}

function Write-BrdfLut {
    param([string]$Path)
    if ( Test-Path $Path ) { return }

    $size = 512
    $bmp  = New-Object System.Drawing.Bitmap $size, $size
    $pi   = [Math]::PI

    for ( $y = 0; $y -lt $size; $y++ ) {
        $roughness = ( $y + 0.5 ) / $size
        $r         = $roughness + 1.0
        $k         = ( $r * $r ) / 8.0
        for ( $x = 0; $x -lt $size; $x++ ) {
            $NdotV = [Math]::Max( ( $x + 0.5 ) / $size, 0.0001 )
            $A     = 1.0 - $k
            $B     = $k
            $scale = $A * $A * $NdotV
            $bias  = $B * ( 1.0 - $NdotV )
            $r8    = [byte]([Math]::Min(255, [Math]::Max(0, [int]($scale * 255.0))))
            $g8    = [byte]([Math]::Min(255, [Math]::Max(0, [int]($bias * 255.0))))
            $bmp.SetPixel($x, $size - 1 - $y, [System.Drawing.Color]::FromArgb(255, $r8, $g8, 0))
        }
    }
    Save-Png -Bitmap $bmp -Path $Path
    $bmp.Dispose()
}

Write-CubemapFaces -SubDir "irradiance" -Size 32
Write-CubemapFaces -SubDir "prefilter" -Size 128
Write-CubemapFaces -SubDir "sky" -Size 128
Write-BrdfLut -Path (Join-Path $envRoot "brdf_lut.png")

$manifestPath = Join-Path $envRoot "manifest.json"
if ( -not ( Test-Path $manifestPath ) ) {
    @"
{
  "schemaVersion": 1,
  "name": "default",
  "irradiance": "irradiance",
  "prefilter": "prefilter",
  "brdfLut": "brdf_lut.png",
  "sky": "sky"
}
"@ | Set-Content -Path $manifestPath -Encoding UTF8
}

Write-Host "IBL default assets ready under $envRoot"
