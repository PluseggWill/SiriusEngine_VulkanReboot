#include "Gfx_ResourceManifest.h"

#include "../Util/Util_DemoAssets.h"

void Gfx_BuildDemoResourceManifest( Gfx_ResourceManifest& aOut ) {
    aOut.myMeshes.clear();
    aOut.myTextures.clear();
    aOut.myMaterials.clear();

    aOut.myMeshes.push_back( { 0, std::string( UtilDemoAssets::kHouseModel ) } );
    aOut.myMeshes.push_back( { 1, std::string( UtilDemoAssets::kMonkeyModel ) } );

    aOut.myTextures.push_back( { 0, std::string( UtilDemoAssets::kDemoTexture ) } );

    Gfx_MaterialManifestEntry material{};
    material.myId                   = 0;
    material.myTextureId            = 0;
    material.myPipelinePermutationId = 0;
    aOut.myMaterials.push_back( material );
}
