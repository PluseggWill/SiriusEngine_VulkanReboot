#include <array>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Vk_ResourceContext.h"
#include "Vk_Types.h"

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

namespace {

std::vector< glm::vec3 > ComputeSmoothNormals( const tinyobj::attrib_t& attrib, const std::vector< tinyobj::shape_t >& shapes ) {
    const size_t             vertexCount = attrib.vertices.size() / 3;
    std::vector< glm::vec3 > normals( vertexCount, glm::vec3( 0.0f ) );

    for ( const auto& shape : shapes ) {
        for ( size_t i = 0; i + 2 < shape.mesh.indices.size(); i += 3 ) {
            const auto& idx0 = shape.mesh.indices[ i ];
            const auto& idx1 = shape.mesh.indices[ i + 1 ];
            const auto& idx2 = shape.mesh.indices[ i + 2 ];

            const glm::vec3 p0 = { attrib.vertices[ 3 * idx0.vertex_index + 0 ], attrib.vertices[ 3 * idx0.vertex_index + 1 ], attrib.vertices[ 3 * idx0.vertex_index + 2 ] };
            const glm::vec3 p1 = { attrib.vertices[ 3 * idx1.vertex_index + 0 ], attrib.vertices[ 3 * idx1.vertex_index + 1 ], attrib.vertices[ 3 * idx1.vertex_index + 2 ] };
            const glm::vec3 p2 = { attrib.vertices[ 3 * idx2.vertex_index + 0 ], attrib.vertices[ 3 * idx2.vertex_index + 1 ], attrib.vertices[ 3 * idx2.vertex_index + 2 ] };

            const glm::vec3 faceNormal = glm::cross( p1 - p0, p2 - p0 );
            if ( glm::dot( faceNormal, faceNormal ) > 0.0f ) {
                normals[ idx0.vertex_index ] += faceNormal;
                normals[ idx1.vertex_index ] += faceNormal;
                normals[ idx2.vertex_index ] += faceNormal;
            }
        }
    }

    for ( auto& normal : normals ) {
        if ( glm::dot( normal, normal ) > 0.0f ) {
            normal = glm::normalize( normal );
        }
        else {
            normal = glm::vec3( 0.0f, 1.0f, 0.0f );
        }
    }

    return normals;
}

}  // namespace

VkVertexInputBindingDescription Gfx_Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof( Gfx_Vertex );
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array< VkVertexInputAttributeDescription, 4 > Gfx_Vertex::getAttributeDescriptions() {
    std::array< VkVertexInputAttributeDescription, 4 > attributeDescriptions{};
    attributeDescriptions[ 0 ].binding  = 0;
    attributeDescriptions[ 0 ].location = 0;
    attributeDescriptions[ 0 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 0 ].offset   = offsetof( Gfx_Vertex, pos );

    attributeDescriptions[ 1 ].binding  = 0;
    attributeDescriptions[ 1 ].location = 1;
    attributeDescriptions[ 1 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 1 ].offset   = offsetof( Gfx_Vertex, color );

    attributeDescriptions[ 2 ].binding  = 0;
    attributeDescriptions[ 2 ].location = 2;
    attributeDescriptions[ 2 ].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[ 2 ].offset   = offsetof( Gfx_Vertex, texCoord );

    attributeDescriptions[ 3 ].binding  = 0;
    attributeDescriptions[ 3 ].location = 3;
    attributeDescriptions[ 3 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 3 ].offset   = offsetof( Gfx_Vertex, normal );

    return attributeDescriptions;
}

void Gfx_Mesh::LoadMesh( const std::string& aPath ) {
    tinyobj::attrib_t                          attrib;
    std::vector< tinyobj::shape_t >            shapes;
    std::vector< tinyobj::material_t >         materials;
    std::string                                warn, error;
    std::unordered_map< Gfx_Vertex, uint32_t > uniqueVertices{};

    if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &error, aPath.c_str() ) ) {
        throw std::runtime_error( warn + error );
    }

    const bool hasObjNormals   = !attrib.normals.empty();
    const auto computedNormals = hasObjNormals ? std::vector< glm::vec3 >{} : ComputeSmoothNormals( attrib, shapes );

    for ( const auto& shape : shapes ) {
        for ( const auto& index : shape.mesh.indices ) {
            Gfx_Vertex vertex{};
            vertex.pos   = { attrib.vertices[ 3 * index.vertex_index + 0 ], attrib.vertices[ 3 * index.vertex_index + 1 ], attrib.vertices[ 3 * index.vertex_index + 2 ] };
            vertex.color = { 1.0f, 1.0f, 1.0f };
            // WARNING: tinyobj uses texcoord_index == -1 when UVs are missing; do not index attrib.texcoords blindly.
            if ( index.texcoord_index >= 0 && !attrib.texcoords.empty() ) {
                vertex.texCoord = { attrib.texcoords[ 2 * index.texcoord_index + 0 ], 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ] };
            }
            else {
                vertex.texCoord = { 0.0f, 0.0f };
            }
            if ( index.normal_index >= 0 ) {
                vertex.normal = { attrib.normals[ 3 * index.normal_index + 0 ], attrib.normals[ 3 * index.normal_index + 1 ], attrib.normals[ 3 * index.normal_index + 2 ] };
            }
            else {
                vertex.normal = computedNormals[ index.vertex_index ];
            }

            if ( uniqueVertices.count( vertex ) == 0 ) {
                uniqueVertices[ vertex ] = static_cast< uint32_t >( myVertices.size() );
                myVertices.push_back( vertex );
            }

            myIndices.push_back( uniqueVertices[ vertex ] );
        }
    }
}

void Gfx_Mesh::BuildBuffers( const Vk_ResourceContext& aContext ) {
    BuildVertexBuffer( aContext );
    BuildIndexBuffer( aContext );
}

void Gfx_Mesh::BuildVertexBuffer( const Vk_ResourceContext& aContext ) {
    const VkDeviceSize bufferSize = sizeof( Gfx_Vertex ) * myVertices.size();

    Vk_AllocatedBuffer stagingBuffer;

    // Staging (CPU) -> device-local vertex buffer via CopyBuffer on transfer queue.
    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer, true );

    void* data;
    vmaMapMemory( aContext.myAllocator, stagingBuffer.myAllocation, &data );
    memcpy( data, myVertices.data(), bufferSize );
    vmaUnmapMemory( aContext.myAllocator, stagingBuffer.myAllocation );

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, myVertexBuffer, false );

    aContext.CopyBuffer( stagingBuffer.myBuffer, myVertexBuffer.myBuffer, bufferSize );

    // Staging must outlive batched CopyBuffer; DestroyStagingBuffer frees after EndSceneUploadBatch.
    aContext.DestroyStagingBuffer( stagingBuffer );
}

void Gfx_Mesh::BuildIndexBuffer( const Vk_ResourceContext& aContext ) {
    const VkDeviceSize bufferSize = sizeof( uint32_t ) * myIndices.size();

    Vk_AllocatedBuffer stagingBuffer;

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer, true );

    void* data;
    vmaMapMemory( aContext.myAllocator, stagingBuffer.myAllocation, &data );
    memcpy( data, myIndices.data(), bufferSize );
    vmaUnmapMemory( aContext.myAllocator, stagingBuffer.myAllocation );

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, myIndexBuffer, false );

    aContext.CopyBuffer( stagingBuffer.myBuffer, myIndexBuffer.myBuffer, bufferSize );

    aContext.DestroyStagingBuffer( stagingBuffer );
}
