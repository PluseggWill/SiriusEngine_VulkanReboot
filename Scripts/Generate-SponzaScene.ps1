#Requires -Version 5.1
<#
.SYNOPSIS
  Split McGuire Sponza OBJ by material and generate Data/Scenes/sponza.json.

  Engine: Z-up. Source Sponza is Y-up (3ds Max); apply R_x(-90 deg) + 0.01 scale
  and recenter so the floor sits near Z=0.

  Requires: Data/Models/sponza/source/ from Fetch-SponzaMcGuire.ps1.
#>
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$SourceDir = Join-Path $RepoRoot "Data\Models\sponza\source"
$PartsDir  = Join-Path $RepoRoot "Data\Models\sponza\parts"
$ObjPath   = Join-Path $SourceDir "sponza.obj"
$MtlPath   = Join-Path $SourceDir "sponza.mtl"
$OutPath   = Join-Path $RepoRoot "Data\Scenes\sponza.json"

if ( -not ( Test-Path $ObjPath ) ) {
    throw "Missing $ObjPath — run Scripts/Fetch-SponzaMcGuire.ps1 first."
}

function ConvertTo-SafeId {
    param( [string]$Name )
    $safe = ( $Name.ToLower() -replace '[^a-z0-9]+', '_' ).Trim( '_' )
    if ( [string]::IsNullOrWhiteSpace( $safe ) ) {
        $safe = "unnamed"
    }
    return "sponza_$safe"
}

function Read-SponzaMtl {
    param( [string]$Path )
    $result  = @{}
    $current = $null
    foreach ( $line in Get-Content -Path $Path ) {
        if ( $line -match '^newmtl\s+(.+)$' ) {
            $current = $matches[1].Trim()
            $result[ $current ] = @{
                diffuseTexture = $null
                hasMask        = $false
            }
        }
        elseif ( $null -ne $current -and $line -match 'map_Kd\s+(.+)$' ) {
            $tex = $matches[1].Trim() -replace '\\', '/'
            $result[ $current ].diffuseTexture = $tex
        }
        elseif ( $null -ne $current -and $line -match 'map_d\s+' ) {
            $result[ $current ].hasMask = $true
        }
    }
    return $result
}

function Write-SplitObj {
    param(
        [string]$OutFile,
        [System.Collections.Generic.List[string]]$Vertices,
        [System.Collections.Generic.List[string]]$TexCoords,
        [System.Collections.Generic.List[string]]$Normals,
        [System.Collections.Generic.List[string]]$Faces
    )

    $vMap  = @{}
    $vtMap = @{}
    $vnMap = @{}
    $outV  = [System.Collections.Generic.List[string]]::new()
    $outVt = [System.Collections.Generic.List[string]]::new()
    $outVn = [System.Collections.Generic.List[string]]::new()
    $outF  = [System.Collections.Generic.List[string]]::new()

    function Register-Index {
        param(
            [hashtable]$Map,
            [System.Collections.Generic.List[string]]$Source,
            [System.Collections.Generic.List[string]]$Target,
            [int]$Index
        )
        if ( $Index -lt 0 ) {
            return 0
        }
        $key = $Index
        if ( -not $Map.ContainsKey( $key ) ) {
            $Map[ $key ] = $Target.Count + 1
            $Target.Add( $Source[ $Index - 1 ] )
        }
        return $Map[ $key ]
    }

    foreach ( $face in $Faces ) {
        $tokens = $face.Substring( 2 ).Trim() -split '\s+'
        $remapped = [System.Collections.Generic.List[string]]::new()
        foreach ( $token in $tokens ) {
            $parts = $token -split '/'
            $vi    = [int]$parts[0]
            $vti   = if ( $parts.Count -gt 1 -and $parts[1] ) { [int]$parts[1] } else { -1 }
            $vni   = if ( $parts.Count -gt 2 -and $parts[2] ) { [int]$parts[2] } else { -1 }
            $newVi  = Register-Index -Map $vMap -Source $Vertices -Target $outV -Index $vi
            $newVti = Register-Index -Map $vtMap -Source $TexCoords -Target $outVt -Index $vti
            $newVni = Register-Index -Map $vnMap -Source $Normals -Target $outVn -Index $vni
            if ( $newVti -gt 0 -and $newVni -gt 0 ) {
                $remapped.Add( "$newVi/$newVti/$newVni" )
            }
            elseif ( $newVti -gt 0 ) {
                $remapped.Add( "$newVi/$newVti" )
            }
            else {
                $remapped.Add( "$newVi" )
            }
        }
        $outF.Add( "f $($remapped -join ' ')" )
    }

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine( "# Split from McGuire Sponza by Generate-SponzaScene.ps1" )
    foreach ( $v in $outV ) { [void]$sb.AppendLine( $v ) }
    foreach ( $vt in $outVt ) { [void]$sb.AppendLine( $vt ) }
    foreach ( $vn in $outVn ) { [void]$sb.AppendLine( $vn ) }
    foreach ( $f in $outF ) { [void]$sb.AppendLine( $f ) }
    [System.IO.File]::WriteAllText( $OutFile, $sb.ToString(), [System.Text.UTF8Encoding]::new( $false ) )
}

function Get-SponzaSceneFrame {
    param( [double]$Scale = 0.01 )
    $minX = [double]::PositiveInfinity
    $minY = [double]::PositiveInfinity
    $minZ = [double]::PositiveInfinity
    $maxX = [double]::NegativeInfinity
    $maxY = [double]::NegativeInfinity
    $maxZ = [double]::NegativeInfinity

    foreach ( $line in [System.IO.File]::ReadLines( $ObjPath ) ) {
        if ( $line -match '^v\s+([\-\d\.eE]+)\s+([\-\d\.eE]+)\s+([\-\d\.eE]+)' ) {
            $x = [double]$matches[1] * $Scale
            $y = [double]$matches[2] * $Scale
            $z = [double]$matches[3] * $Scale
            # R_x(-90 deg): (x, y, z) -> (x, z, -y)
            $wx = $x
            $wy = $z
            $wz = -$y
            if ( $wx -lt $minX ) { $minX = $wx }
            if ( $wy -lt $minY ) { $minY = $wy }
            if ( $wz -lt $minZ ) { $minZ = $wz }
            if ( $wx -gt $maxX ) { $maxX = $wx }
            if ( $wy -gt $maxY ) { $maxY = $wy }
            if ( $wz -gt $maxZ ) { $maxZ = $wz }
        }
    }

    $tx = -( $minX + $maxX ) / 2.0
    $ty = -( $minY + $maxY ) / 2.0
    $tz = -$minZ
    $S  = $Scale

    $worldMinX = $minX + $tx
    $worldMaxX = $maxX + $tx
    $worldMinY = $minY + $ty
    $worldMaxY = $maxY + $ty
    $worldMinZ = $minZ + $tz
    $worldMaxZ = $maxZ + $tz

    return @{
        transform = @(
            $S, 0.0, 0.0, 0.0,
            0.0, 0.0, $S, 0.0,
            0.0, -$S, 0.0, 0.0,
            $tx, $ty, $tz, 1.0
        )
        centerX = 0.0
        centerY = 0.0
        centerZ = ( $worldMinZ + $worldMaxZ ) / 2.0
        extentX = $worldMaxX - $worldMinX
        extentY = $worldMaxY - $worldMinY
        extentZ = $worldMaxZ - $worldMinZ
        floorZ  = $worldMinZ
    }
}

Write-Host "[GEN] Parsing $ObjPath"
$vertices  = [System.Collections.Generic.List[string]]::new()
$texCoords = [System.Collections.Generic.List[string]]::new()
$normals   = [System.Collections.Generic.List[string]]::new()
$facesByMat = @{}
$currentMat = "default"

foreach ( $line in [System.IO.File]::ReadLines( $ObjPath ) ) {
    if ( $line.StartsWith( 'v ' ) ) {
        $vertices.Add( $line )
    }
    elseif ( $line.StartsWith( 'vt ' ) ) {
        $texCoords.Add( $line )
    }
    elseif ( $line.StartsWith( 'vn ' ) ) {
        $normals.Add( $line )
    }
    elseif ( $line -match '^usemtl\s+(.+)$' ) {
        $currentMat = $matches[1].Trim()
    }
    elseif ( $line.StartsWith( 'f ' ) ) {
        if ( -not $facesByMat.ContainsKey( $currentMat ) ) {
            $facesByMat[ $currentMat ] = [System.Collections.Generic.List[string]]::new()
        }
        $facesByMat[ $currentMat ].Add( $line )
    }
}

if ( Test-Path $PartsDir ) {
    Remove-Item -Recurse -Force $PartsDir
}
New-Item -ItemType Directory -Path $PartsDir -Force | Out-Null

$mtlInfo = Read-SponzaMtl -Path $MtlPath
$meshes      = [System.Collections.Generic.List[object]]::new()
$textures    = [System.Collections.Generic.List[object]]::new()
$materials   = [System.Collections.Generic.List[object]]::new()
$logicalList = [System.Collections.Generic.List[object]]::new()
$entities    = [System.Collections.Generic.List[object]]::new()
$textureIds  = @{}
$sceneFrame  = Get-SponzaSceneFrame
$rootXform   = $sceneFrame.transform
$eyeHeight   = 1.75
$spawnEyeZ   = $sceneFrame.floorZ + $eyeHeight
$lookAheadY  = [Math]::Round( $sceneFrame.extentY * 0.32, 2 )
$lookTargetZ = [Math]::Round( $spawnEyeZ + 0.8, 2 )
$overviewZ   = [Math]::Round( $sceneFrame.extentZ * 0.55, 2 )

foreach ( $matName in ( $facesByMat.Keys | Sort-Object ) ) {
    $faces = $facesByMat[ $matName ]
    if ( $faces.Count -eq 0 ) {
        continue
    }

    $meshId    = ConvertTo-SafeId -Name $matName
    $partFile  = Join-Path $PartsDir "$meshId.obj"
    Write-SplitObj -OutFile $partFile -Vertices $vertices -TexCoords $texCoords -Normals $normals -Faces $faces

    $meshes.Add( @{
            id   = $meshId
            path = "Data/Models/sponza/parts/$meshId.obj"
        } )
    $logicalList.Add( @{
            id        = $meshId
            lodMeshes = @($meshId)
        } )

    $matId = "mat_$meshId"
    $texId = $null
    $matEntry = [ordered]@{
        id     = $matId
        shader = "lit"
    }

    $info = $null
    if ( $mtlInfo.ContainsKey( $matName ) ) {
        $info = $mtlInfo[ $matName ]
    }

    if ( $null -ne $info -and $info.diffuseTexture ) {
        $relTex = $info.diffuseTexture -replace '^textures/', ''
        $texId  = "tex_$meshId"
        if ( -not $textureIds.ContainsKey( $relTex ) ) {
            $textureIds[ $relTex ] = $texId
            $textures.Add( @{
                    id   = $texId
                    path = "Data/Models/sponza/source/textures/$relTex"
                } )
        }
        else {
            $texId = $textureIds[ $relTex ]
        }
        $matEntry.texture = $texId
    }
    else {
        $matEntry.texture  = "tex_fallback_stone"
        $matEntry.baseColor = @(0.55, 0.52, 0.48, 1.0)
    }

    if ( $null -ne $info -and $info.hasMask ) {
        $matEntry.alpha       = 0.85
        $matEntry.transparent = $true
    }

    $materials.Add( $matEntry )
    $entity = [ordered]@{
        logicalMesh = $meshId
        material    = $matId
        transform   = $rootXform
    }
    if ( $matEntry.transparent ) {
        $entity.renderFlags = "transparent"
    }
    $entities.Add( $entity )
}

if ( -not ( $textureIds.ContainsKey( "spnza_bricks_a_diff.png" ) ) ) {
    $textures.Add( @{
            id   = "tex_fallback_stone"
            path = "Data/Models/sponza/source/textures/spnza_bricks_a_diff.png"
        } )
}
elseif ( -not ( $textures | Where-Object { $_.id -eq "tex_fallback_stone" } ) ) {
    $textures.Add( @{
            id   = "tex_fallback_stone"
            path = "Data/Models/sponza/source/textures/spnza_bricks_a_diff.png"
        } )
}

$scene = [ordered]@{
    version       = 1
    name          = "sponza"
    shaders       = @{
        lit = @{
            vert = "VulkanDesktop/Shader_Generated/TriangleVert.spv"
            frag = "VulkanDesktop/Shader_Generated/TrianglePix.spv"
        }
    }
    logicalMeshes = $logicalList
    meshes        = $meshes
    textures      = $textures
    materials     = $materials
    cameras       = @(
        @{
            id       = "spawn"
            eye      = @( $sceneFrame.centerX, $sceneFrame.centerY, $spawnEyeZ )
            center   = @( $sceneFrame.centerX, $lookAheadY, $lookTargetZ )
            up       = @(0.0, 0.0, 1.0)
            fovDeg   = 60.0
            viewport = @(0.0, 0.0, 1.0, 1.0)
        },
        @{
            id       = "overview"
            eye      = @( $sceneFrame.centerX, $sceneFrame.centerY, $overviewZ )
            center   = @( $sceneFrame.centerX, $lookAheadY, $sceneFrame.centerZ )
            up       = @(0.0, 0.0, 1.0)
            fovDeg   = 55.0
            viewport = @(0.66, 0.66, 0.32, 0.32)
        }
    )
    entities      = $entities
}

$json = $scene | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText( $OutPath, $json, [System.Text.UTF8Encoding]::new( $false ) )
Write-Host "[GEN] Wrote $OutPath parts=$($meshes.Count) entities=$($entities.Count) textures=$($textures.Count)"
