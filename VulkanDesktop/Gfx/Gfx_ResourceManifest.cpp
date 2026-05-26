#include "Gfx_ResourceManifest.h"

#include "../Util/Util_DemoAssets.h"

namespace {

Gfx_MaterialManifestEntry MakeOpaqueMaterial( uint32_t aId, uint32_t aTextureId ) {
    Gfx_MaterialManifestEntry entry{};
    entry.myId                    = aId;
    entry.myTextureId             = aTextureId;
    entry.myPipelinePermutationId = 0;
    return entry;
}

}  // namespace

// See Gfx_ResourceManifest.h — shipped path is Gfx_BuildResourceManifestFromSceneDesc (Gfx_SceneApply.cpp).
void Gfx_BuildDemoResourceManifest( Gfx_ResourceManifest& aOut ) {
    aOut.myMeshes.clear();
    aOut.myTextures.clear();
    aOut.myMaterials.clear();

    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshViking, std::string( UtilDemoAssets::kHouseModel ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshMonkey, std::string( UtilDemoAssets::kMonkeyModel ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshTreeDetailed, std::string( UtilDemoAssets::kKenneyTreeDetailed ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshTreeSimple, std::string( UtilDemoAssets::kKenneyTreeSimple ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshRock, std::string( UtilDemoAssets::kKenneyRockLarge ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshCampfire, std::string( UtilDemoAssets::kKenneyCampfire ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshTent, std::string( UtilDemoAssets::kKenneyTent ) } );
    aOut.myMeshes.push_back( { UtilDemoAssets::kMeshStump, std::string( UtilDemoAssets::kKenneyStump ) } );

    aOut.myTextures.push_back( { 0, std::string( UtilDemoAssets::kDemoTexture ) } );
    aOut.myTextures.push_back( { 1, std::string( UtilDemoAssets::kAltTexture ) } );
    aOut.myTextures.push_back( { 2, std::string( UtilDemoAssets::kTexRock ) } );
    aOut.myTextures.push_back( { 3, std::string( UtilDemoAssets::kTexGrass ) } );
    aOut.myTextures.push_back( { 4, std::string( UtilDemoAssets::kTexMetal ) } );
    aOut.myTextures.push_back( { 5, std::string( UtilDemoAssets::kTexWood ) } );

    aOut.myMaterials.push_back( MakeOpaqueMaterial( UtilDemoAssets::kMatViking, 0 ) );
    aOut.myMaterials.push_back( MakeOpaqueMaterial( UtilDemoAssets::kMatMonkey, 1 ) );

    Gfx_MaterialManifestEntry transparent{};
    transparent.myId                    = UtilDemoAssets::kMatTransparent;
    transparent.myTextureId             = 0;
    transparent.myPipelinePermutationId = 0;
    transparent.myAlpha                 = 0.35f;
    transparent.myIsTransparent         = true;
    aOut.myMaterials.push_back( transparent );

    aOut.myMaterials.push_back( MakeOpaqueMaterial( UtilDemoAssets::kMatRock, 2 ) );
    aOut.myMaterials.push_back( MakeOpaqueMaterial( UtilDemoAssets::kMatGrass, 3 ) );
    aOut.myMaterials.push_back( MakeOpaqueMaterial( UtilDemoAssets::kMatMetal, 4 ) );
    aOut.myMaterials.push_back( MakeOpaqueMaterial( UtilDemoAssets::kMatWood, 5 ) );
}
