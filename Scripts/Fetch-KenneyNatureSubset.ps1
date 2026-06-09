#Requires -Version 5.1
<#
.SYNOPSIS
  Download Kenney Nature Kit (CC0) and copy a curated OBJ subset into Data/Models/.
.DESCRIPTION
  Idempotent: skips download when extracted cache exists; only copies missing repo files.
  Source: OpenGameArt mirror of Kenney Nature Kit 2.1.
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$zipUrl      = "https://opengameart.org/sites/default/files/Nature%20Kit%20%282.1%29.zip"
$zipPath     = Join-Path $env:TEMP "kenney_nature_kit.zip"
$extractPath = Join-Path $env:TEMP "kenney_nature_extract"
$objSource   = Join-Path $extractPath "Models\OBJ format"
$destDir     = Join-Path $RepoRoot "Data\Models"

# Kenney source name -> repo filename (kenney_* prefix convention).
$copyMap = @{
    # Terrain / water / cliffs / bridge (stress valley layout)
    "ground_grass.obj"            = "kenney_ground_grass.obj"
    "ground_riverStraight.obj"    = "kenney_ground_river_straight.obj"
    "ground_riverCorner.obj"      = "kenney_ground_river_corner.obj"
    "ground_pathStraight.obj"     = "kenney_ground_path_straight.obj"
    "cliff_rock.obj"              = "kenney_cliff_rock.obj"
    "cliff_large_rock.obj"        = "kenney_cliff_large_rock.obj"
    "cliff_corner_rock.obj"       = "kenney_cliff_corner_rock.obj"
    "cliff_waterfall_rock.obj"    = "kenney_cliff_waterfall.obj"
    "cliff_waterfallTop_rock.obj" = "kenney_cliff_waterfall_top.obj"
    "bridge_side_stone.obj"       = "kenney_bridge_side_stone.obj"
    "bridge_center_stone.obj"     = "kenney_bridge_center_stone.obj"
    # Trees / settlement props
    "tree_detailed.obj"           = "kenney_tree_detailed.obj"
    "tree_simple.obj"             = "kenney_tree_simple.obj"
    "tree_tall.obj"               = "kenney_tree_tall.obj"
    "tree_oak.obj"                = "kenney_tree_oak.obj"
    "tree_small.obj"              = "kenney_tree_small.obj"
    "tree_pineDefaultA.obj"       = "kenney_tree_pine_a.obj"
    "tree_pineSmallA.obj"         = "kenney_tree_pine_simple.obj"
    "plant_bushLarge.obj"         = "kenney_bush_large.obj"
    "plant_bushSmall.obj"         = "kenney_bush_small.obj"
    "rock_largeA.obj"             = "kenney_rock_large_a.obj"
    "rock_smallA.obj"             = "kenney_rock_small_a.obj"
    "campfire_stones.obj"         = "kenney_campfire_stones.obj"
    "tent_detailedClosed.obj"     = "kenney_tent_closed.obj"
    "log_stack.obj"               = "kenney_log_stack.obj"
    "path_stone.obj"              = "kenney_path_stone.obj"
    "stump_round.obj"             = "kenney_stump_round.obj"
}

if ( -not ( Test-Path $objSource ) ) {
    if ( -not ( Test-Path $zipPath ) ) {
        Write-Host "[FETCH] Downloading Kenney Nature Kit..."
        Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
    }
    Write-Host "[FETCH] Extracting to $extractPath"
    if ( Test-Path $extractPath ) {
        Remove-Item -Recurse -Force $extractPath
    }
    Expand-Archive -Path $zipPath -DestinationPath $extractPath -Force
}

if ( -not ( Test-Path $objSource ) ) {
    throw "OBJ source folder not found: $objSource"
}

if ( -not ( Test-Path $destDir ) ) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

$copied  = 0
$skipped = 0
foreach ( $entry in $copyMap.GetEnumerator() ) {
    $src = Join-Path $objSource $entry.Key
    $dst = Join-Path $destDir $entry.Value
    if ( -not ( Test-Path $src ) ) {
        throw "Missing Kenney source OBJ: $($entry.Key)"
    }
    if ( Test-Path $dst ) {
        $skipped++
        continue
    }
    Copy-Item -Path $src -Destination $dst
    Write-Host "[FETCH] Copied $($entry.Value)"
    $copied++
}

Write-Host "[FETCH] Done. copied=$copied skipped=$skipped dest=$destDir"
