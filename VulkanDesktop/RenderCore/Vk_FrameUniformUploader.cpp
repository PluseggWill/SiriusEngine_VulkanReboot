#include "Vk_FrameUniformUploader.h"

#include "Vk_Core.h"

#include <cstring>

#include <glm/glm.hpp>

void Vk_FrameUniformUploader::UpdateForView( const Vk_Core& aCore, uint32_t aCurrentFrame, uint32_t aViewIndex, const Vk_Camera& aCamera ) {
    GpuCameraData cam{};
    cam.view = aCamera.myView;
    cam.proj = aCamera.myProj;

    void* data = nullptr;
    vmaMapMemory( aCore.myDeviceCtx.myAllocator, aCore.myFrameCtx.myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation, &data );
    const size_t cameraStride = aCore.PadUniformBufferSize( sizeof( GpuCameraData ) );
    memcpy( static_cast< char* >( data ) + cameraStride * aViewIndex, &cam, sizeof( cam ) );
    vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, aCore.myFrameCtx.myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation );
}

void Vk_FrameUniformUploader::Update( const Vk_Core& aCore, uint32_t aCurrentFrame ) {
    UpdateForView( aCore, aCurrentFrame, 0, aCore.myCamera );

    GpuEnvironmentData env    = aCore.myEnvironmentData;
    const glm::vec3    sunDir = glm::vec3( env.mySunlightDirection );
    if ( glm::dot( sunDir, sunDir ) > 0.0001f ) {
        env.mySunlightDirection = glm::vec4( glm::normalize( sunDir ), 0.0f );
    }
    env.myViewWorldPos = glm::vec4( aCore.myCamera.myEye, 1.0f );

    char* mapGpuEnvData = nullptr;
    vmaMapMemory( aCore.myDeviceCtx.myAllocator, aCore.myEnvDataBuffer.myAllocation, ( void** )&mapGpuEnvData );
    mapGpuEnvData += aCore.PadUniformBufferSize( sizeof( GpuEnvironmentData ) ) * aCurrentFrame;
    memcpy( mapGpuEnvData, &env, sizeof( GpuEnvironmentData ) );
    vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, aCore.myEnvDataBuffer.myAllocation );
}
