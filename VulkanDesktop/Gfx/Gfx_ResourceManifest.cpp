#include "Gfx_ResourceManifest.h"

#include "../Util/Util_DemoAssets.h"

void Gfx_BuildDemoResourceManifest( Gfx_ResourceManifest& aOut ) {
    aOut.myMeshes.clear();
    aOut.myTextures.clear();
    aOut.myMaterials.clear();

    aOut.myMeshes.push_back( { 0, std::string( UtilDemoAssets::kHouseModel ) } );
    aOut.myMeshes.push_back( { 1, std::string( UtilDemoAssets::kMonkeyModel ) } );

    aOut.myTextures.push_back( { 0, std::string( UtilDemoAssets::kDemoTexture ) } );
    aOut.myTextures.push_back( { 1, std::string( UtilDemoAssets::kAltTexture ) } );

    Gfx_MaterialManifestEntry material0{};
    material0.myId                    = 0;
    material0.myTextureId             = 0;
    material0.myPipelinePermutationId = 0;
    aOut.myMaterials.push_back( material0 );

    Gfx_MaterialManifestEntry material1{};
    material1.myId                    = 1;
    material1.myTextureId             = 1;
    material1.myPipelinePermutationId = 0;
    aOut.myMaterials.push_back( material1 );
}
