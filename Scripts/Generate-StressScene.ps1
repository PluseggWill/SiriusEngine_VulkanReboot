#Requires -Version 5.1
<#
.SYNOPSIS
  Generate Data/Scenes/stress.json — valley settlement stress scene.

  Engine: Z-up; walkable plane = X-Y at Z=0 (Vk_Camera forwardFlat).
  Kenney OBJ: Y-up -> R_x(+90 deg) so model +Y maps to world +Z (not buried).

  Placement uses a cell occupancy map so grass, props, and trees never share a tile.
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$TileScale = 5.0
$BridgeScale = 2.0
# Z layers (world Z-up) — separate coplanar ground types to avoid depth fighting.
$GrassZ      = 0.0
$PathZ       = 0.03 * $TileScale
$RiverBedZ   = -0.06 * $TileScale
$BridgeDeckZ = 0.05 * $TileScale

# Column-major T * R_z(yaw) * R_x(+90 deg) * S
function New-KenneyTransform {
    param(
        [float]$X,
        [float]$Y,
        [float]$Z = 0.0,
        [float]$Scale = 1.0,
        [float]$YawDeg = 0.0
    )
    $rad = [float]( $YawDeg * [Math]::PI / 180.0 )
    $c   = [Math]::Cos( $rad )
    $s   = [Math]::Sin( $rad )
    $S   = $Scale
    return @(
        ($c * $S), ($s * $S), 0.0, 0.0,
        0.0, 0.0, $S, 0.0,
        ($s * $S), (-$c * $S), 0.0, 0.0,
        $X, $Y, $Z, 1.0
    )
}

function New-WorldTransform {
    param(
        [float]$X,
        [float]$Y,
        [float]$Z = 0.0,
        [float]$Scale = 1.0,
        [float]$YawDeg = 0.0
    )
    $rad = [float]( $YawDeg * [Math]::PI / 180.0 )
    $c   = [Math]::Cos( $rad )
    $s   = [Math]::Sin( $rad )
    $S   = $Scale
    return @(
        ($c * $S), ($s * $S), 0.0, 0.0,
        (-$s * $S), ($c * $S), 0.0, 0.0,
        0.0, 0.0, $S, 0.0,
        $X, $Y, $Z, 1.0
    )
}

function New-Entity {
    param(
        [string]$LogicalMesh,
        [string]$Material,
        [float[]]$Transform,
        [string]$RenderFlags = "opaque"
    )
    $entity = [ordered]@{
        logicalMesh = $LogicalMesh
        material    = $Material
        transform   = $Transform
    }
    if ( $RenderFlags -ne "opaque" ) {
        $entity.renderFlags = $RenderFlags
    }
    return $entity
}

function Get-CellKey {
    param( [int]$Col, [int]$Row )
    return "$Col,$Row"
}

$occupied = @{}
$entities = [System.Collections.Generic.List[object]]::new()

function Mark-Cell {
    param( [int]$Col, [int]$Row )
    $occupied[ ( Get-CellKey -Col $Col -Row $Row ) ] = $true
}

function Test-CellFree {
    param( [int]$Col, [int]$Row )
    return -not $occupied.ContainsKey( ( Get-CellKey -Col $Col -Row $Row ) )
}

function Add-AtCell {
    param(
        [string]$LogicalMesh,
        [string]$Material,
        [int]$Col,
        [int]$Row,
        [float]$YawDeg = 0.0,
        [float]$Scale = $TileScale,
        [float]$Z = 0.0,
        [float]$JitterX = 0.0,
        [float]$JitterY = 0.0,
        [switch]$Kenney,
        [switch]$World,
        [string]$RenderFlags = "opaque"
    )
    $x = [float]$Col * $TileScale + $JitterX
    $y = [float]$Row * $TileScale + $JitterY
    if ( -not ( Test-CellFree -Col $Col -Row $Row ) ) {
        return
    }
    if ( $Kenney ) {
        $xf = New-KenneyTransform -X $x -Y $y -Z $Z -Scale $Scale -YawDeg $YawDeg
    }
    else {
        $xf = New-WorldTransform -X $x -Y $y -Z $Z -Scale $Scale -YawDeg $YawDeg
    }
    $entities.Add( ( New-Entity -LogicalMesh $LogicalMesh -Material $Material -Transform $xf -RenderFlags $RenderFlags ) )
    Mark-Cell -Col $Col -Row $Row
}

# --- West-bank forest first (cols -5..-3 only; never shares bridge row cells) ---
$forest = @(
    @( "tree_giant", -5, -4 ), @( "tree_oak", -4, -4 ), @( "pine", -3, -4 ),
    @( "tree", -5, -5 ), @( "tree_giant", -4, -5 ), @( "bush_large", -3, -5 ),
    @( "pine", -5, -6 ), @( "tree_oak", -4, -6 ), @( "tree", -3, -6 ),
    @( "bush_small", -5, -7 ), @( "tree_giant", -4, -7 ), @( "rock_small", -3, -7 )
)
foreach ( $f in $forest ) {
    $yaw = ( [Math]::Abs( $f[1] * 17 + $f[2] * 31 ) ) % 360
    $scale = if ( $f[0] -match 'tree_giant' ) { 3.2 } elseif ( $f[0] -match 'tree|oak|pine' ) { 2.8 } elseif ( $f[0] -match 'bush' ) { 1.8 } else { 1.5 }
    Add-AtCell -LogicalMesh $f[0] -Material $( if ( $f[0] -match 'rock' ) { 'mat_rock' } else { 'mat_foliage' } ) -Col $f[1] -Row $f[2] -Kenney -Scale $scale -YawDeg $yaw
}

# --- East-bank settlement ---
Add-AtCell -LogicalMesh "house" -Material "mat_viking" -Col 4 -Row -2 -JitterX 1.0 -YawDeg 90 -World -Scale 1.0 -Z 0.12
Add-AtCell -LogicalMesh "tent" -Material "mat_tent" -Col 5 -Row -3 -JitterX -1.5 -Kenney -Scale 2.4
Add-AtCell -LogicalMesh "campfire" -Material "mat_stone" -Col 3 -Row -2 -JitterX -1.0 -Kenney -Scale 2.0
Add-AtCell -LogicalMesh "log_stack" -Material "mat_wood" -Col 4 -Row -1 -JitterY 1.2 -Kenney -Scale 1.8 -YawDeg 20
Add-AtCell -LogicalMesh "stump" -Material "mat_wood" -Col 5 -Row -1 -Kenney -Scale 1.6

foreach ( $step in @( @( 3, -2 ), @( 3, -3 ), @( 2, -4 ) ) ) {
    Add-AtCell -LogicalMesh "path_tile" -Material "mat_path" -Col $step[0] -Row $step[1] -Z $PathZ -Kenney
}

# --- River (col 0) ---
foreach ( $row in -4, -5, -6, -7, -8 ) {
    Add-AtCell -LogicalMesh "ground_river" -Material "mat_water" -Col 0 -Row $row -Z $RiverBedZ -Kenney
}

# --- Cliff wall (row -9); waterfall cols -1,0; tops on row -10 ---
$cliffRow = -9
foreach ( $col in -5..5 ) {
    if ( $col -eq -5 ) {
        Add-AtCell -LogicalMesh "cliff_corner" -Material "mat_cliff" -Col $col -Row $cliffRow -Kenney
        continue
    }
    if ( $col -eq 5 ) {
        Add-AtCell -LogicalMesh "cliff_corner" -Material "mat_cliff" -Col $col -Row $cliffRow -YawDeg 180 -Kenney
        continue
    }
    if ( $col -in -1, 0 ) {
        Add-AtCell -LogicalMesh "cliff_waterfall" -Material "mat_cliff" -Col $col -Row $cliffRow -Kenney
        Add-AtCell -LogicalMesh "cliff_waterfall_top" -Material "mat_cliff" -Col $col -Row ( $cliffRow - 1 ) -Kenney
        continue
    }
    if ( $col -in -4, 4 ) {
        Add-AtCell -LogicalMesh "cliff_large" -Material "mat_cliff" -Col $col -Row $cliffRow -Kenney
        continue
    }
    Add-AtCell -LogicalMesh "cliff_wall" -Material "mat_cliff" -Col $col -Row $cliffRow -Kenney
}

# --- Compact bridge (row -4): 4 pieces spanning river, scale 2 m ---
$bridgeRow = -4
Add-AtCell -LogicalMesh "bridge_side" -Material "mat_bridge" -Col -2 -Row $bridgeRow -Z $BridgeDeckZ -Scale $BridgeScale -Kenney
Add-AtCell -LogicalMesh "bridge_span" -Material "mat_bridge" -Col -1 -Row $bridgeRow -Z $BridgeDeckZ -Scale $BridgeScale -Kenney
Add-AtCell -LogicalMesh "bridge_span" -Material "mat_bridge" -Col 1 -Row $bridgeRow -Z $BridgeDeckZ -Scale $BridgeScale -Kenney
Add-AtCell -LogicalMesh "bridge_side" -Material "mat_bridge" -Col 2 -Row $bridgeRow -Z $BridgeDeckZ -Scale $BridgeScale -YawDeg 180 -Kenney

# --- River bank rocks (east side) ---
foreach ( $r in @( @( "rock_large", 2, -6 ), @( "rock_small", 2, -5 ), @( "rock_large", 1, -7 ) ) ) {
    Add-AtCell -LogicalMesh $r[0] -Material "mat_rock" -Col $r[1] -Row $r[2] -Kenney -Scale 1.8 -JitterX 1.0
}

# --- Grass: fill meadow cells not yet occupied ---
foreach ( $row in -1, -2, -3, -4, -6, -7, -8 ) {
    foreach ( $col in -5..5 ) {
        if ( -not ( Test-CellFree -Col $col -Row $row ) ) { continue }
        # Keep a clear margin beside river column except far south bank.
        if ( $col -eq 0 ) { continue }
        Add-AtCell -LogicalMesh "ground_grass" -Material "mat_grass" -Col $col -Row $row -Z $GrassZ -Kenney
    }
}

# Mist near waterfall pool (world space, no cell).
$entities.Add( ( New-Entity -LogicalMesh "mist_marker" -Material "mat_mist" -Transform ( New-WorldTransform -X 0 -Y ( -8 * $TileScale ) -Z 2.5 -Scale 0.9 ) -RenderFlags "transparent" ) )

$scene = [ordered]@{
    version = 1
    name    = "stress"
    shaders = [ordered]@{
        lit = [ordered]@{
            vert = "VulkanDesktop/Shader_Generated/TriangleVert.spv"
            frag = "VulkanDesktop/Shader_Generated/TrianglePix.spv"
        }
    }
    logicalMeshes = @(
        @{ id = "ground_grass"; lodMeshes = @("kenney_ground_grass") }
        @{ id = "ground_river"; lodMeshes = @("kenney_ground_river") }
        @{ id = "cliff_wall"; lodMeshes = @("kenney_cliff_wall") }
        @{ id = "cliff_large"; lodMeshes = @("kenney_cliff_large") }
        @{ id = "cliff_corner"; lodMeshes = @("kenney_cliff_corner") }
        @{ id = "cliff_waterfall"; lodMeshes = @("kenney_cliff_waterfall") }
        @{ id = "cliff_waterfall_top"; lodMeshes = @("kenney_cliff_waterfall_top") }
        @{ id = "bridge_side"; lodMeshes = @("kenney_bridge_side") }
        @{ id = "bridge_span"; lodMeshes = @("kenney_bridge_span") }
        @{ id = "path_tile"; lodMeshes = @("kenney_path_tile") }
        @{ id = "house"; lodMeshes = @("viking_room") }
        @{ id = "tent"; lodMeshes = @("kenney_tent") }
        @{ id = "campfire"; lodMeshes = @("kenney_campfire") }
        @{ id = "log_stack"; lodMeshes = @("kenney_log_stack") }
        @{ id = "stump"; lodMeshes = @("kenney_stump") }
        @{ id = "tree"; lodMeshes = @("kenney_tree_detailed", "kenney_tree_simple"); lodDistances = @(18.0) }
        @{ id = "tree_giant"; lodMeshes = @("kenney_tree_tall", "kenney_tree_simple"); lodDistances = @(20.0) }
        @{ id = "pine"; lodMeshes = @("kenney_tree_pine_a", "kenney_tree_pine_simple"); lodDistances = @(16.0) }
        @{ id = "tree_oak"; lodMeshes = @("kenney_tree_oak", "kenney_tree_small"); lodDistances = @(18.0) }
        @{ id = "bush_large"; lodMeshes = @("kenney_bush_large") }
        @{ id = "bush_small"; lodMeshes = @("kenney_bush_small") }
        @{ id = "rock_large"; lodMeshes = @("kenney_rock_large") }
        @{ id = "rock_small"; lodMeshes = @("kenney_rock_small") }
        @{ id = "mist_marker"; lodMeshes = @("monkey") }
    )
    meshes = @(
        @{ id = "kenney_ground_grass"; path = "Data/Models/kenney_ground_grass.obj" }
        @{ id = "kenney_ground_river"; path = "Data/Models/kenney_ground_river_straight.obj" }
        @{ id = "kenney_cliff_wall"; path = "Data/Models/kenney_cliff_rock.obj" }
        @{ id = "kenney_cliff_large"; path = "Data/Models/kenney_cliff_large_rock.obj" }
        @{ id = "kenney_cliff_corner"; path = "Data/Models/kenney_cliff_corner_rock.obj" }
        @{ id = "kenney_cliff_waterfall"; path = "Data/Models/kenney_cliff_waterfall.obj" }
        @{ id = "kenney_cliff_waterfall_top"; path = "Data/Models/kenney_cliff_waterfall_top.obj" }
        @{ id = "kenney_bridge_side"; path = "Data/Models/kenney_bridge_side_stone.obj" }
        @{ id = "kenney_bridge_span"; path = "Data/Models/kenney_bridge_center_stone.obj" }
        @{ id = "kenney_path_tile"; path = "Data/Models/kenney_ground_path_straight.obj" }
        @{ id = "viking_room"; path = "Data/Models/viking_room.obj" }
        @{ id = "kenney_tent"; path = "Data/Models/kenney_tent_closed.obj" }
        @{ id = "kenney_campfire"; path = "Data/Models/kenney_campfire_stones.obj" }
        @{ id = "kenney_log_stack"; path = "Data/Models/kenney_log_stack.obj" }
        @{ id = "kenney_stump"; path = "Data/Models/kenney_stump_round.obj" }
        @{ id = "kenney_tree_detailed"; path = "Data/Models/kenney_tree_detailed.obj" }
        @{ id = "kenney_tree_simple"; path = "Data/Models/kenney_tree_simple.obj" }
        @{ id = "kenney_tree_tall"; path = "Data/Models/kenney_tree_tall.obj" }
        @{ id = "kenney_tree_pine_a"; path = "Data/Models/kenney_tree_pine_a.obj" }
        @{ id = "kenney_tree_pine_simple"; path = "Data/Models/kenney_tree_pine_simple.obj" }
        @{ id = "kenney_tree_oak"; path = "Data/Models/kenney_tree_oak.obj" }
        @{ id = "kenney_tree_small"; path = "Data/Models/kenney_tree_small.obj" }
        @{ id = "kenney_bush_large"; path = "Data/Models/kenney_bush_large.obj" }
        @{ id = "kenney_bush_small"; path = "Data/Models/kenney_bush_small.obj" }
        @{ id = "kenney_rock_large"; path = "Data/Models/kenney_rock_large_a.obj" }
        @{ id = "kenney_rock_small"; path = "Data/Models/kenney_rock_small_a.obj" }
        @{ id = "monkey"; path = "Data/Models/monkey_smooth.obj" }
    )
    textures = @(
        @{ id = "viking_albedo"; path = "Data/Textures/viking_room.png" }
        @{ id = "tex_rock"; path = "Data/Textures/ph_rock_diff_1k.jpg" }
        @{ id = "tex_grass"; path = "Data/Textures/ph_grass_diff_1k.jpg" }
        @{ id = "tex_metal"; path = "Data/Textures/ph_metal_diff_1k.jpg" }
        @{ id = "tex_wood"; path = "Data/Textures/ph_wood_table_diff_1k.jpg" }
    )
    materials = @(
        @{ id = "mat_viking"; shader = "lit"; texture = "viking_albedo" }
        @{ id = "mat_grass"; shader = "lit"; texture = "tex_grass"; baseColor = @(0.17, 0.85, 0.72, 1.0) }
        @{ id = "mat_foliage"; shader = "lit"; texture = "tex_grass"; baseColor = @(0.16, 0.79, 0.67, 1.0) }
        @{ id = "mat_cliff"; shader = "lit"; texture = "tex_rock"; baseColor = @(0.89, 0.51, 0.34, 1.0); roughness = 0.95 }
        @{ id = "mat_rock"; shader = "lit"; texture = "tex_rock"; baseColor = @(0.72, 0.89, 0.91, 1.0) }
        @{ id = "mat_bridge"; shader = "lit"; texture = "tex_rock"; baseColor = @(0.72, 0.89, 0.91, 1.0); roughness = 0.85 }
        @{ id = "mat_water"; shader = "lit"; texture = "tex_rock"; baseColor = @(0.69, 0.96, 1.0, 1.0); roughness = 0.12 }
        @{ id = "mat_wood"; shader = "lit"; texture = "tex_wood"; baseColor = @(0.89, 0.51, 0.34, 1.0) }
        @{ id = "mat_tent"; shader = "lit"; texture = "tex_wood"; baseColor = @(0.88, 0.29, 0.31, 1.0) }
        @{ id = "mat_stone"; shader = "lit"; texture = "tex_rock"; baseColor = @(0.72, 0.89, 0.91, 1.0) }
        @{ id = "mat_path"; shader = "lit"; texture = "tex_rock"; baseColor = @(0.96, 0.84, 0.73, 1.0); roughness = 0.88 }
        @{ id = "mat_mist"; shader = "lit"; texture = "viking_albedo"; alpha = 0.25; transparent = $true }
    )
    cameras = @(
        @{
            id       = "overview"
            eye      = @(50.0, -30.0, 40.0)
            center   = @(0.0, -30.0, 0.0)
            up       = @(0.0, 0.0, 1.0)
            fovDeg   = 52.0
            viewport = @(0.66, 0.66, 0.32, 0.32)
        }
    )
    entities = $entities
}

$outPath = Join-Path $RepoRoot "Data\Scenes\stress.json"
$json    = $scene | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText( $outPath, $json, [System.Text.UTF8Encoding]::new( $false ) )
Write-Host "[GEN] Wrote $outPath entities=$($entities.Count) occupiedCells=$($occupied.Count)"
