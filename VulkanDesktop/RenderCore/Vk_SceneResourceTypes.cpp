#include "Vk_SceneResourceTypes.h"

#include "Vk_ResourceContext.h"

#include <cstring>

void Vk_MeshResource::BuildGpuBuffers( const Vk_ResourceContext& aContext ) {
    BuildVertexBuffer( aContext );
    BuildIndexBuffer( aContext );
}

void Vk_MeshResource::BuildVertexBuffer( const Vk_ResourceContext& aContext ) {
    const VkDeviceSize bufferSize = sizeof( Gfx_Vertex ) * myCpu.myVertices.size();

    Vk_AllocatedBuffer stagingBuffer;

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer, true );

    void* data;
    vmaMapMemory( aContext.myAllocator, stagingBuffer.myAllocation, &data );
    memcpy( data, myCpu.myVertices.data(), bufferSize );
    vmaUnmapMemory( aContext.myAllocator, stagingBuffer.myAllocation );

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, myVertexBuffer, false );

    aContext.CopyBuffer( stagingBuffer.myBuffer, myVertexBuffer.myBuffer, bufferSize );

    aContext.DestroyStagingBuffer( stagingBuffer );
}

void Vk_MeshResource::BuildIndexBuffer( const Vk_ResourceContext& aContext ) {
    myIndexCount                  = static_cast< uint32_t >( myCpu.myIndices.size() );
    const VkDeviceSize bufferSize = sizeof( uint32_t ) * static_cast< size_t >( myIndexCount );

    Vk_AllocatedBuffer stagingBuffer;

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer, true );

    void* data;
    vmaMapMemory( aContext.myAllocator, stagingBuffer.myAllocation, &data );
    memcpy( data, myCpu.myIndices.data(), bufferSize );
    vmaUnmapMemory( aContext.myAllocator, stagingBuffer.myAllocation );

    aContext.CreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, myIndexBuffer, false );

    aContext.CopyBuffer( stagingBuffer.myBuffer, myIndexBuffer.myBuffer, bufferSize );

    aContext.DestroyStagingBuffer( stagingBuffer );
}
