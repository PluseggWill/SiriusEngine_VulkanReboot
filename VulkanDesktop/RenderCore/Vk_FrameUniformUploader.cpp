#include "Vk_FrameUniformUploader.h"

#include "Vk_Core.h"

#include "Vk_IblResources.h"

#include "../Gfx/Gfx_LightingGlobals.h"

#include "../Gfx/Gfx_LightingMath.h"

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

void Vk_FrameUniformUploader::UpdateEnvironment( const Vk_Core& aCore, uint32_t aCurrentFrame ) {

    GpuEnvironmentData env = aCore.myEnvironmentData;

    const glm::vec3 sunDir = glm::vec3( env.mySunlightDirection );

    if ( glm::dot( sunDir, sunDir ) > 0.0001f ) {

        env.mySunlightDirection = glm::vec4( glm::normalize( sunDir ), 0.0f );
    }

    // Env eye follows primary fly camera (not per-view scene cameras).

    env.myViewWorldPos = glm::vec4( aCore.myCamera.myEye, 1.0f );

    char* mapGpuEnvData = nullptr;

    vmaMapMemory( aCore.myDeviceCtx.myAllocator, aCore.myEnvDataBuffer.myAllocation, ( void** )&mapGpuEnvData );

    mapGpuEnvData += aCore.PadUniformBufferSize( sizeof( GpuEnvironmentData ) ) * aCurrentFrame;

    memcpy( mapGpuEnvData, &env, sizeof( GpuEnvironmentData ) );

    vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, aCore.myEnvDataBuffer.myAllocation );
}

void Vk_FrameUniformUploader::UpdateLightingGlobals( const Vk_Core& aCore, uint32_t aCurrentFrame, const glm::mat4& aLightViewProj ) {

    const float prefilterMaxMip = aCore.myIblResourcesState.myInitialized ? aCore.myIblResourcesState.myPrefilterMaxMipLevel : 0.0f;

    GpuLightingGlobals globals = Gfx_BuildLightingGlobals( aCore.myLightingSettings, aLightViewProj, prefilterMaxMip );

    char* mapData = nullptr;

    vmaMapMemory( aCore.myDeviceCtx.myAllocator, aCore.myLightingGlobalsBuffer.myAllocation, ( void** )&mapData );

    mapData += aCore.PadUniformBufferSize( sizeof( GpuLightingGlobals ) ) * aCurrentFrame;

    memcpy( mapData, &globals, sizeof( GpuLightingGlobals ) );

    vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, aCore.myLightingGlobalsBuffer.myAllocation );
}

void Vk_FrameUniformUploader::UpdateLightingGlobalsFromScene( const Vk_Core& aCore, uint32_t aCurrentFrame ) {

    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );

    const Gfx_Bounds sceneBounds = aCore.GetShadowCasterBounds();

    const glm::mat4 lightViewProj = Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowMatrixFromScene( sunDir, sceneBounds );

    UpdateLightingGlobals( aCore, aCurrentFrame, lightViewProj );
}
