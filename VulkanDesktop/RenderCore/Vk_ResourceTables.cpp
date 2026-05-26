#include "Vk_ResourceTables.h"

#include <algorithm>

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_DataStruct.h"

void Vk_ResourceTables::Clear() {
    myMeshes.clear();
    myMaterials.clear();
    myTextures.clear();
    myMaterialTextureIds.clear();
    myMaterialTableGeneration = 0;
}

void Vk_ResourceTables::LoadFromManifest( const Gfx_ResourceManifest& aManifest, Vk_Core& aCore, Vk_DeletionQueue& aDeletionQueue, uint32_t& aTextureMipLevels,
                                          VkPipeline aOpaquePipeline, VkPipeline aTransparentPipeline, VkPipelineLayout aLayout ) {
    Clear();

    myMeshes.reserve( aManifest.myMeshes.size() );
    myTextures.reserve( aManifest.myTextures.size() );
    myMaterials.reserve( aManifest.myMaterials.size() );
    myMaterialTextureIds.reserve( aManifest.myMaterials.size() );

    for ( const Gfx_TextureManifestEntry& entry : aManifest.myTextures ) {
        uint32_t textureMipLevels = 1;
        LoadTexture( entry.myPath, entry.myId, aCore, aDeletionQueue, textureMipLevels );
        aTextureMipLevels = std::max( aTextureMipLevels, textureMipLevels );
    }

    for ( const Gfx_MeshManifestEntry& entry : aManifest.myMeshes ) {
        LoadMesh( entry.myPath, entry.myId, aCore, aDeletionQueue );
    }

    for ( const Gfx_MaterialManifestEntry& entry : aManifest.myMaterials ) {
        VkPipeline pipeline = entry.myIsTransparent ? aTransparentPipeline : aOpaquePipeline;
        CreateMaterialEntry( entry.myId, entry.myTextureId, pipeline, aLayout, entry.myAlpha );
    }

    ++myMaterialTableGeneration;

    UtilLogger::Info( "RESOURCE-TABLE",
                      "meshes=" + std::to_string( myMeshes.size() ) + " materials=" + std::to_string( myMaterials.size() ) +
                          " textures=" + std::to_string( myTextures.size() ) + " materialTableGeneration=" +
                          std::to_string( myMaterialTableGeneration ) );
}

Gfx_Mesh* Vk_ResourceTables::LoadMesh( const std::string& aPath, uint32_t aMeshId, Vk_Core& aCore, Vk_DeletionQueue& aDeletionQueue ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aPath );
    UtilLogger::Info( "RESOURCE", "Loading mesh id=" + std::to_string( aMeshId ) + " path=" + resolvedPath );

    if ( aMeshId >= myMeshes.size() ) {
        myMeshes.resize( aMeshId + 1 );
    }

    Gfx_Mesh& mesh = myMeshes[ aMeshId ];
    mesh.LoadMesh( resolvedPath );
    mesh.BuildBuffers();

    aDeletionQueue.pushFunction( [ &aCore, aMeshId, this ]() {
        if ( aMeshId >= myMeshes.size() ) {
            return;
        }
        Gfx_Mesh& toDestroy = myMeshes[ aMeshId ];
        vmaDestroyBuffer( aCore.myAllocator, toDestroy.myVertexBuffer.myBuffer, toDestroy.myVertexBuffer.myAllocation );
        vmaDestroyBuffer( aCore.myAllocator, toDestroy.myIndexBuffer.myBuffer, toDestroy.myIndexBuffer.myAllocation );
    } );

    return &mesh;
}

Gfx_Texture* Vk_ResourceTables::LoadTexture( const std::string& aPath, uint32_t aTextureId, Vk_Core& aCore, Vk_DeletionQueue& aDeletionQueue, uint32_t& aMipLevels ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aPath );
    UtilLogger::Info( "RESOURCE", "Loading texture id=" + std::to_string( aTextureId ) + " path=" + resolvedPath );

    if ( aTextureId >= myTextures.size() ) {
        myTextures.resize( aTextureId + 1 );
    }

    Gfx_Texture& texture = myTextures[ aTextureId ];
    if ( UtilLoader::LoadTexture( resolvedPath, texture, aMipLevels ) != true ) {
        throw std::runtime_error( "failed to load texture!" );
    }
    texture.ImageView() = aCore.CreateImageView( texture.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, aMipLevels );

    aDeletionQueue.pushFunction( [ &aCore, aTextureId, this ]() {
        if ( aTextureId >= myTextures.size() ) {
            return;
        }
        Gfx_Texture& toDestroy = myTextures[ aTextureId ];
        vmaDestroyImage( aCore.myAllocator, toDestroy.Image(), toDestroy.Allocation() );
        vkDestroyImageView( aCore.myDevice, toDestroy.ImageView(), nullptr );
    } );

    return &texture;
}

Gfx_Material* Vk_ResourceTables::CreateMaterialEntry( uint32_t aMaterialId, uint32_t aTextureId, VkPipeline aPipeline, VkPipelineLayout aLayout, float aAlpha ) {
    if ( aMaterialId >= myMaterials.size() ) {
        myMaterials.resize( aMaterialId + 1 );
    }

    Gfx_Material& material    = myMaterials[ aMaterialId ];
    material.myPipeline       = aPipeline;
    material.myPipelineLayout = aLayout;
    material.myAlpha          = aAlpha;

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
