#include "Vk_ResourceTables.h"

#include <algorithm>
#include <stdexcept>

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_DataStruct.h"
#include "Vk_ResourceContext.h"

void Vk_ResourceTables::Clear() {
    myMeshes.clear();
    myMaterials.clear();
    myTextures.clear();
    myMaterialTextureIds.clear();
    myMaterialTableGeneration = 0;
}

void Vk_ResourceTables::LoadFromManifest( const Gfx_ResourceManifest& aManifest, const Vk_ResourceContext& aContext, Vk_DeletionQueue& aSceneDeletionQueue,
                                          uint32_t& aTextureMipLevels, VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline, VkPipelineLayout aLayout ) {
    Clear();

    myMeshes.reserve( aManifest.myMeshes.size() );
    myTextures.reserve( aManifest.myTextures.size() );
    myMaterials.reserve( aManifest.myMaterials.size() );
    myMaterialTextureIds.reserve( aManifest.myMaterials.size() );

    for ( const Gfx_TextureManifestEntry& entry : aManifest.myTextures ) {
        uint32_t textureMipLevels = 1;
        LoadTexture( entry.myPath, entry.myId, aContext, aSceneDeletionQueue, textureMipLevels );
        aTextureMipLevels = std::max( aTextureMipLevels, textureMipLevels );
    }

    for ( const Gfx_MeshManifestEntry& entry : aManifest.myMeshes ) {
        LoadMesh( entry.myPath, entry.myId, aContext, aSceneDeletionQueue );
    }

    for ( const Gfx_MaterialManifestEntry& entry : aManifest.myMaterials ) {
        VkPipeline pipeline = entry.myIsTransparent ? aTransparentPipeline : aOpaquePipeline;
        CreateMaterialEntry( entry.myId, entry.myTextureId, pipeline, aLayout, entry.myAlpha, entry.myIsTransparent );
    }

    ++myMaterialTableGeneration;

    UtilLogger::Info( "RESOURCE-TABLE",
                      "meshes=" + std::to_string( myMeshes.size() ) + " materials=" + std::to_string( myMaterials.size() ) +
                          " textures=" + std::to_string( myTextures.size() ) + " materialTableGeneration=" +
                          std::to_string( myMaterialTableGeneration ) );
}

Gfx_Mesh* Vk_ResourceTables::LoadMesh( const std::string& aPath, uint32_t aMeshId, const Vk_ResourceContext& aContext, Vk_DeletionQueue& aSceneDeletionQueue ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aPath );
    UtilLogger::Info( "RESOURCE", "Loading mesh id=" + std::to_string( aMeshId ) + " path=" + resolvedPath );

    if ( aMeshId >= myMeshes.size() ) {
        myMeshes.resize( aMeshId + 1 );
    }

    Gfx_Mesh& mesh = myMeshes[ aMeshId ];
    mesh.LoadMesh( resolvedPath );
    mesh.BuildBuffers( aContext );

    const VmaAllocator allocator = aContext.myAllocator;
    aSceneDeletionQueue.pushFunction( [ allocator, aMeshId, this ]() {
        if ( aMeshId >= myMeshes.size() ) {
            return;
        }
        Gfx_Mesh& toDestroy = myMeshes[ aMeshId ];
        vmaDestroyBuffer( allocator, toDestroy.myVertexBuffer.myBuffer, toDestroy.myVertexBuffer.myAllocation );
        vmaDestroyBuffer( allocator, toDestroy.myIndexBuffer.myBuffer, toDestroy.myIndexBuffer.myAllocation );
    } );

    return &mesh;
}

Gfx_Texture* Vk_ResourceTables::LoadTexture( const std::string& aPath, uint32_t aTextureId, const Vk_ResourceContext& aContext, Vk_DeletionQueue& aSceneDeletionQueue,
                                             uint32_t& aMipLevels ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aPath );
    UtilLogger::Info( "RESOURCE", "Loading texture id=" + std::to_string( aTextureId ) + " path=" + resolvedPath );

    if ( aTextureId >= myTextures.size() ) {
        myTextures.resize( aTextureId + 1 );
    }

    Gfx_Texture& texture = myTextures[ aTextureId ];
    if ( UtilLoader::LoadTexture( resolvedPath, aContext, texture, aMipLevels ) != true ) {
        throw std::runtime_error( "failed to load texture!" );
    }
    texture.ImageView() = aContext.CreateImageView( texture.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, aMipLevels );

    const VmaAllocator allocator = aContext.myAllocator;
    const VkDevice     device    = aContext.myDevice;
    aSceneDeletionQueue.pushFunction( [ allocator, device, aTextureId, this ]() {
        if ( aTextureId >= myTextures.size() ) {
            return;
        }
        Gfx_Texture& toDestroy = myTextures[ aTextureId ];
        vmaDestroyImage( allocator, toDestroy.Image(), toDestroy.Allocation() );
        vkDestroyImageView( device, toDestroy.ImageView(), nullptr );
    } );

    return &texture;
}

void Vk_ResourceTables::RefreshMaterialPipelines( VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline, VkPipelineLayout aLayout ) {
    for ( Gfx_Material& material : myMaterials ) {
        if ( material.myPipeline == VK_NULL_HANDLE ) {
            continue;
        }
        material.myPipeline       = material.myIsTransparent ? aTransparentPipeline : aOpaquePipeline;
        material.myPipelineLayout = aLayout;
    }
}

Gfx_Material* Vk_ResourceTables::CreateMaterialEntry( uint32_t aMaterialId, uint32_t aTextureId, VkPipeline aPipeline, VkPipelineLayout aLayout, float aAlpha,
                                                    bool aIsTransparent ) {
    if ( aMaterialId >= myMaterials.size() ) {
        myMaterials.resize( aMaterialId + 1 );
    }

    Gfx_Material& material    = myMaterials[ aMaterialId ];
    material.myPipeline       = aPipeline;
    material.myPipelineLayout = aLayout;
    material.myAlpha          = aAlpha;
    material.myIsTransparent  = aIsTransparent;

    if ( aMaterialId >= myMaterialTextureIds.size() ) {
        myMaterialTextureIds.resize( aMaterialId + 1, UINT32_MAX );
    }
    myMaterialTextureIds[ aMaterialId ] = aTextureId;

    return &material;
}

const Gfx_Mesh& Vk_ResourceTables::GetMesh( uint32_t aMeshId ) const {
    if ( aMeshId >= myMeshes.size() || myMeshes[ aMeshId ].myIndices.empty() ) {
        throw std::runtime_error( "Vk_ResourceTables: invalid mesh id " + std::to_string( aMeshId ) );
    }
    return myMeshes[ aMeshId ];
}

const Gfx_Material& Vk_ResourceTables::GetMaterial( uint32_t aMaterialId ) const {
    if ( aMaterialId >= myMaterials.size() || myMaterials[ aMaterialId ].myPipeline == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_ResourceTables: invalid material id " + std::to_string( aMaterialId ) );
    }
    return myMaterials[ aMaterialId ];
}

const Gfx_Texture& Vk_ResourceTables::GetTexture( uint32_t aTextureId ) const {
    if ( aTextureId >= myTextures.size() || myTextures[ aTextureId ].ImageView() == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_ResourceTables: invalid texture id " + std::to_string( aTextureId ) );
    }
    return myTextures[ aTextureId ];
}

uint32_t Vk_ResourceTables::GetTextureIdForMaterial( uint32_t aMaterialId ) const {
    if ( aMaterialId >= myMaterialTextureIds.size() ) {
        throw std::runtime_error( "Vk_ResourceTables: invalid material id for texture lookup " + std::to_string( aMaterialId ) );
    }
    return myMaterialTextureIds[ aMaterialId ];
}
