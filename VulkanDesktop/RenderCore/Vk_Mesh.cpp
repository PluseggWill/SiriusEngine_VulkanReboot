#include "Vk_Mesh.h"
#include "Vk_Core.h"

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

// Vertex:
VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof( Vertex );
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array< VkVertexInputAttributeDescription, 3 > Vertex::getAttributeDescriptions() {
    std::array< VkVertexInputAttributeDescription, 3 > attributeDescriptions{};
    attributeDescriptions[ 0 ].binding  = 0;
    attributeDescriptions[ 0 ].location = 0;
    attributeDescriptions[ 0 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 0 ].offset   = offsetof( Vertex, pos );

    attributeDescriptions[ 1 ].binding  = 0;
    attributeDescriptions[ 1 ].location = 1;
    attributeDescriptions[ 1 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 1 ].offset   = offsetof( Vertex, color );

    attributeDescriptions[ 2 ].binding  = 0;
    attributeDescriptions[ 2 ].location = 2;
    attributeDescriptions[ 2 ].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[ 2 ].offset   = offsetof( Vertex, texCoord );

    return attributeDescriptions;
}

void Mesh::LoadMesh( const std::string& aPath ) {
    // Fill the data by loading model
    tinyobj::attrib_t                      attrib;
    std::vector< tinyobj::shape_t >        shapes;
    std::vector< tinyobj::material_t >     materials;
    std::string                            warn, error;
    std::unordered_map< Vertex, uint32_t > uniqueVertices{};

    if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &error, aPath.c_str() ) ) {
        throw std::runtime_error( warn + error );
    }

    for ( const auto& shape : shapes ) {
        for ( const auto& index : shape.mesh.indices ) {
            Vertex vertex{};
            vertex.pos      = { attrib.vertices[ 3 * index.vertex_index + 0 ], attrib.vertices[ 3 * index.vertex_index + 1 ], attrib.vertices[ 3 * index.vertex_index + 2 ] };
            vertex.texCoord = { attrib.texcoords[ 2 * index.texcoord_index + 0 ], 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ] };
            vertex.color    = { 1.0f, 1.0f, 1.0f };

            if ( uniqueVertices.count( vertex ) == 0 ) {
                uniqueVertices[ vertex ] = static_cast< uint32_t >( myVertices.size() );
                myVertices.push_back( vertex );
            }

            myIndices.push_back( uniqueVertices[ vertex ] );
        }
    }
}

void Mesh::BuildBuffers() {
    BuildVertexBuffer();
    BuildIndexBuffer();
}

void Mesh::BuildVertexBuffer() {
    const VkDeviceSize bufferSize = sizeof( Vertex ) * myVertices.size();

    AllocatedBuffer stagingBuffer;

    // The staging buffer memory is allocated at device host, which can be accessed by CPU, but less optimal
    Vk_Core::GetInstance().CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer, true );

    void* data;
    vmaMapMemory( Vk_Core::GetInstance().myAllocator, stagingBuffer.myAllocation, &data );
    memcpy( data, myVertices.data(), bufferSize );
    vmaUnmapMemory( Vk_Core::GetInstance().myAllocator, stagingBuffer.myAllocation );

    // The vertex buffer is, on the other hand, allocated on the device local, which can only be accessed by GPU and offers better performance
    Vk_Core::GetInstance().CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, myVertexBuffer,
                                         false );

    Vk_Core::GetInstance().CopyBuffer( stagingBuffer.myBuffer, myVertexBuffer.myBuffer, bufferSize );

    vmaDestroyBuffer( Vk_Core::GetInstance().myAllocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );
}

void Mesh::BuildIndexBuffer() {
    const VkDeviceSize bufferSize = sizeof( uint32_t ) * myIndices.size();

    AllocatedBuffer stagingBuffer;

    // The staging buffer memory is allocated at device host, which can be accessed by CPU, but less optimal
    Vk_Core::GetInstance().CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer, true );

    void* data;
    vmaMapMemory( Vk_Core::GetInstance().myAllocator, stagingBuffer.myAllocation, &data );
    memcpy( data, myIndices.data(), bufferSize );
    vmaUnmapMemory( Vk_Core::GetInstance().myAllocator, stagingBuffer.myAllocation );

    // The index buffer is, on the other hand, allocated on the device local, which can only be accessed by GPU and offers better performance
    Vk_Core::GetInstance().CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, myIndexBuffer,
                                         false );

    Vk_Core::GetInstance().CopyBuffer( stagingBuffer.myBuffer, myIndexBuffer.myBuffer, bufferSize );

    vmaDestroyBuffer( Vk_Core::GetInstance().myAllocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );
}