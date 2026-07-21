#include "Vk_ResourceTables.h"

#include <algorithm>
#include <stdexcept>

#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_DataStruct.h"
#include "Vk_ResourceContext.h"
#include "Vk_TextureLoader.h"

void Vk_ResourceTables::Clear() {
    myMeshes.clear();
    myMaterials.clear();
    myTextures.clear();
    myMaterialTextureIds.clear();
    myMaterialTableGeneration = 0;
}

void Vk_ResourceTables::LoadFromManifest( const Util_EngineConfig& aConfig, const Gfx_ResourceManifest& aManifest, const Vk_ResourceContext& aContext,
                                          Vk_DeletionQueue& aSceneDeletionQueue, uint32_t& aTextureMipLevels, VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline,
                                          VkPipelineLayout aLayout ) {
    Clear();

    myMeshes.reserve( aManifest.myMeshes.size() );
    myTextures.reserve( aManifest.myTextures.size() );
    myMaterials.reserve( aManifest.myMaterials.size() );
    myMaterialTextureIds.reserve( aManifest.myMaterials.size() );

    for ( const Gfx_TextureManifestEntry& entry : aManifest.myTextures ) {
        uint32_t textureMipLevels = 1;
        LoadTexture( aConfig, entry.myPath, entry.myId, aContext, aSceneDeletionQueue, textureMipLevels );
        aTextureMipLevels = std::max( aTextureMipLevels, textureMipLevels );
    }

    // Batch mesh staging→device copies only; textures above use immediate submit (layout + copy + mips).
    aContext.BeginSceneUploadBatch();

    for ( const Gfx_MeshManifestEntry& entry : aManifest.myMeshes ) {
        LoadMesh( aConfig, entry.myPath, entry.myId, aContext, aSceneDeletionQueue );
    }

    aContext.EndSceneUploadBatch();

    for ( const Gfx_MaterialManifestEntry& entry : aManifest.myMaterials ) {
        VkPipeline pipeline = entry.myIsTransparent ? aTransparentPipeline : aOpaquePipeline;
        CreateMaterialEntry( entry.myId, entry.myTextureId, pipeline, aLayout, entry );
    }

    ++myMaterialTableGeneration;

    UtilLogger::Info( "RESOURCE-TABLE", "meshes=" + std::to_string( myMeshes.size() ) + " materials=" + std::to_string( myMaterials.size() )
                                            + " textures=" + std::to_string( myTextures.size() ) + " materialTableGeneration=" + std::to_string( myMaterialTableGeneration ) );
}

Vk_MeshResource* Vk_ResourceTables::LoadMesh( const Util_EngineConfig& aConfig, const std::string& aPath, uint32_t aMeshId, const Vk_ResourceContext& aContext,
                                              Vk_DeletionQueue& aSceneDeletionQueue ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aConfig, aPath );
    UtilLogger::Info( "RESOURCE", "Loading mesh id=" + std::to_string( aMeshId ) + " path=" + resolvedPath );

    if ( aMeshId >= myMeshes.size() ) {
        myMeshes.resize( aMeshId + 1 );
    }

    Vk_MeshResource& mesh = myMeshes[ aMeshId ];
    mesh.myCpu.LoadFromPath( resolvedPath );
    mesh.BuildGpuBuffers( aContext );

    const VmaAllocator       allocator    = aContext.myAllocator;
    const Vk_AllocatedBuffer vertexBuffer = mesh.myVertexBuffer;
    const Vk_AllocatedBuffer indexBuffer  = mesh.myIndexBuffer;
    aSceneDeletionQueue.pushFunction( [ allocator, vertexBuffer, indexBuffer ]() {
        if ( vertexBuffer.myBuffer != VK_NULL_HANDLE ) {
            vmaDestroyBuffer( allocator, vertexBuffer.myBuffer, vertexBuffer.myAllocation );
        }
        if ( indexBuffer.myBuffer != VK_NULL_HANDLE ) {
            vmaDestroyBuffer( allocator, indexBuffer.myBuffer, indexBuffer.myAllocation );
        }
    } );

    return &mesh;
}

Vk_TextureResource* Vk_ResourceTables::LoadTexture( const Util_EngineConfig& aConfig, const std::string& aPath, uint32_t aTextureId, const Vk_ResourceContext& aContext,
                                                    Vk_DeletionQueue& aSceneDeletionQueue, uint32_t& aMipLevels ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aConfig, aPath );
    UtilLogger::Info( "RESOURCE", "Loading texture id=" + std::to_string( aTextureId ) + " path=" + resolvedPath );

    if ( aTextureId >= myTextures.size() ) {
        myTextures.resize( aTextureId + 1 );
    }

    Vk_TextureResource& texture = myTextures[ aTextureId ];
    if ( Vk_TextureLoader::LoadTexture( aConfig, aPath, aContext, texture, aMipLevels ) != true ) {
        throw std::runtime_error( "failed to load texture!" );
    }

    const VmaAllocator  allocator  = aContext.myAllocator;
    const VkDevice      device     = aContext.myDevice;
    const VkImageView   imageView  = texture.ImageView();
    const VkImage       image      = texture.Image();
    const VmaAllocation allocation = texture.Allocation();
    aSceneDeletionQueue.pushFunction( [ allocator, device, imageView, image, allocation ]() {
        if ( imageView != VK_NULL_HANDLE ) {
            vkDestroyImageView( device, imageView, nullptr );
        }
        if ( image != VK_NULL_HANDLE ) {
            vmaDestroyImage( allocator, image, allocation );
        }
    } );

    return &texture;
}

void Vk_ResourceTables::RefreshMaterialPipelines( VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline, VkPipelineLayout aLayout ) {
    for ( Vk_MaterialResource& material : myMaterials ) {
        if ( material.myPipeline == VK_NULL_HANDLE ) {
            continue;
        }
        material.myPipeline       = material.myIsTransparent ? aTransparentPipeline : aOpaquePipeline;
        material.myPipelineLayout = aLayout;
    }
}

Vk_MaterialResource* Vk_ResourceTables::CreateMaterialEntry( uint32_t aMaterialId, uint32_t aTextureId, VkPipeline aPipeline, VkPipelineLayout aLayout,
                                                             const Gfx_MaterialManifestEntry& aSurface ) {
    if ( aMaterialId >= myMaterials.size() ) {
        myMaterials.resize( aMaterialId + 1 );
    }

    Vk_MaterialResource& material = myMaterials[ aMaterialId ];
    material.myPipeline           = aPipeline;
    material.myPipelineLayout     = aLayout;
    material.myBaseColorFactor    = aSurface.myBaseColorFactor;
    material.myRoughness          = aSurface.myRoughness;
    material.myMetallic           = aSurface.myMetallic;
    material.myAlpha              = aSurface.myAlpha;
    material.myAlphaMode          = aSurface.myAlphaMode;
    material.myIsTransparent      = aSurface.myIsTransparent;

    if ( aMaterialId >= myMaterialTextureIds.size() ) {
        myMaterialTextureIds.resize( aMaterialId + 1, UINT32_MAX );
    }
    myMaterialTextureIds[ aMaterialId ] = aTextureId;

    return &material;
}

const Vk_MeshResource& Vk_ResourceTables::GetMesh( uint32_t aMeshId ) const {
    if ( aMeshId >= myMeshes.size() || myMeshes[ aMeshId ].myCpu.myIndices.empty() ) {
        throw std::runtime_error( "Vk_ResourceTables: invalid mesh id " + std::to_string( aMeshId ) );
    }
    return myMeshes[ aMeshId ];
}

std::vector< Gfx_Bounds > Vk_ResourceTables::CollectMeshLocalBounds() const {
    std::vector< Gfx_Bounds > bounds;
    bounds.reserve( myMeshes.size() );
    for ( const Vk_MeshResource& mesh : myMeshes ) {
        bounds.push_back( mesh.myLocalBounds() );
    }
    return bounds;
}

const Vk_MaterialResource& Vk_ResourceTables::GetMaterial( uint32_t aMaterialId ) const {
    if ( aMaterialId >= myMaterials.size() || myMaterials[ aMaterialId ].myPipeline == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_ResourceTables: invalid material id " + std::to_string( aMaterialId ) );
    }
    return myMaterials[ aMaterialId ];
}

const Vk_TextureResource& Vk_ResourceTables::GetTexture( uint32_t aTextureId ) const {
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
