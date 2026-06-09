#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Gfx/Gfx_ResourceManifest.h"
#include "Vk_Types.h"

struct Util_EngineConfig;
struct Vk_ResourceContext;
struct Vk_DeletionQueue;

// GPU resource tables: stable mesh/material/texture ids → loaded Gfx_* + material→texture link.
// Hot path (extract/draw records) uses ids only; resolve at record/descriptor update boundaries.
class Vk_ResourceTables {
public:
    void Clear();

    // Loads unique paths once; registers GPU teardown on aDeletionQueue. Pipeline/layout applied to all materials v0.
    void LoadFromManifest( const Util_EngineConfig& aConfig, const Gfx_ResourceManifest& aManifest, const Vk_ResourceContext& aContext, Vk_DeletionQueue& aSceneDeletionQueue,
                           uint32_t& aTextureMipLevels, VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline, VkPipelineLayout aLayout );

    const Gfx_Mesh&     GetMesh( uint32_t aMeshId ) const;
    const Gfx_Material& GetMaterial( uint32_t aMaterialId ) const;
    const Gfx_Texture&  GetTexture( uint32_t aTextureId ) const;
    uint32_t            GetTextureIdForMaterial( uint32_t aMaterialId ) const;

    size_t GetMeshCount() const {
        return myMeshes.size();
    }
    size_t GetMaterialCount() const {
        return myMaterials.size();
    }
    size_t GetTextureCount() const {
        return myTextures.size();
    }

    uint16_t GetMaterialTableGeneration() const {
        return myMaterialTableGeneration;
    }

    // After swapchain recreate: Vk pipelines are recreated; refresh stored handles (same material ids).
    void RefreshMaterialPipelines( VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline, VkPipelineLayout aLayout );

private:
    Gfx_Mesh*     LoadMesh( const Util_EngineConfig& aConfig, const std::string& aPath, uint32_t aMeshId, const Vk_ResourceContext& aContext,
                            Vk_DeletionQueue& aSceneDeletionQueue );
    Gfx_Texture*  LoadTexture( const Util_EngineConfig& aConfig, const std::string& aPath, uint32_t aTextureId, const Vk_ResourceContext& aContext,
                               Vk_DeletionQueue& aSceneDeletionQueue, uint32_t& aMipLevels );
    Gfx_Material* CreateMaterialEntry( uint32_t aMaterialId, uint32_t aTextureId, VkPipeline aPipeline, VkPipelineLayout aLayout, const Gfx_MaterialManifestEntry& aSurface );

    std::vector< Gfx_Mesh >     myMeshes;
    std::vector< Gfx_Material > myMaterials;
    std::vector< Gfx_Texture >  myTextures;
    std::vector< uint32_t >     myMaterialTextureIds;
    uint16_t                    myMaterialTableGeneration = 0;
};
