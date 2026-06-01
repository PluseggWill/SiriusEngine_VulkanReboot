#include "Vk_FrameUniformUploader.h"

#include "Vk_Core.h"

#include <cstring>

#include <glm/glm.hpp>

void Vk_FrameUniformUploader::Update( const Vk_Core& aCore, uint32_t aCurrentFrame ) {
    GpuCameraData cam{};
    cam.view = aCore.myCamera.myView;
    cam.proj = aCore.myCamera.myProj;

    void* data = nullptr;
    vmaMapMemory( aCore.myAllocator, aCore.myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation, &data );
    memcpy( data, &cam, sizeof( cam ) );
    vmaUnmapMemory( aCore.myAllocator, aCore.myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation );

    GpuEnvironmentData env    = aCore.myEnvironmentData;
    const glm::vec3    sunDir = glm::vec3( env.mySunlightDirection );
    if ( glm::dot( sunDir, sunDir ) > 0.0001f ) {
        env.mySunlightDirection = glm::vec4( glm::normalize( sunDir ), 0.0f );
    }
    env.myViewWorldPos = glm::vec4( aCore.myCamera.myEye, 1.0f );

    // Env UBO is one big slab; each in-flight frame writes to its aligned slice.
    char* mapGpuEnvData = nullptr;
    vmaMapMemory( aCore.myAllocator, aCore.myEnvDataBuffer.myAllocation, ( void** )&mapGpuEnvData );
    mapGpuEnvData += aCore.PadUniformBufferSize( sizeof( GpuEnvironmentData ) ) * aCurrentFrame;
    memcpy( mapGpuEnvData, &env, sizeof( GpuEnvironmentData ) );
    vmaUnmapMemory( aCore.myAllocator, aCore.myEnvDataBuffer.myAllocation );
}
