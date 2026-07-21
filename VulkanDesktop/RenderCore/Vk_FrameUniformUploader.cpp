#include "Vk_FrameUniformUploader.h"

#include "Vk_ActiveRenderView.h"
#include "Vk_Renderer.h"

#include "Vk_IblResources.h"
#include "Vk_ShadowMapPass.h"

#include "../Gfx/Gfx_LightingGlobals.h"

#include "../Gfx/Gfx_LightingMath.h"

#include <cstring>

#include <glm/glm.hpp>

void Vk_FrameUniformUploader::UpdateForView( const Vk_Renderer& aCore, uint32_t aCurrentFrame, uint32_t aViewIndex, const Vk_ActiveRenderView& aView ) {

    Gpu_CameraData cam{};

    cam.view = aView.myCameraView;

    cam.proj         = aView.myCameraProj;
    cam.currViewProj = aView.myCameraProj * aView.myCameraView;
    if ( aViewIndex == 0 ) {
        cam.prevViewProj = aCore.myTemporalState.myPrevViewProj;
        cam.temporalJitterAndFlags =
            glm::vec4( aCore.myTemporalState.myJitterPixel.x, aCore.myTemporalState.myJitterPixel.y, aCore.myTemporalState.myHistoryValid ? 1.0f : 0.0f, 0.0f );
    }
    else {
        cam.prevViewProj           = cam.currViewProj;
        cam.temporalJitterAndFlags = glm::vec4( 0.0f );
    }

    void* data = nullptr;

    vmaMapMemory( aCore.myRhi.myDeviceCtx.myAllocator, aCore.myFrameCtx.myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation, &data );

    const size_t cameraStride = aCore.PadUniformBufferSize( sizeof( Gpu_CameraData ) );

    memcpy( static_cast< char* >( data ) + cameraStride * aViewIndex, &cam, sizeof( cam ) );

    vmaUnmapMemory( aCore.myRhi.myDeviceCtx.myAllocator, aCore.myFrameCtx.myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation );
}

void Vk_FrameUniformUploader::UpdateEnvironment( const Vk_Renderer& aCore, uint32_t aCurrentFrame ) {

    Gpu_EnvironmentData env = aCore.myEnvironmentData;

    const glm::vec3 sunDir = glm::vec3( env.mySunlightDirection );

    if ( glm::dot( sunDir, sunDir ) > 0.0001f ) {

        env.mySunlightDirection = glm::vec4( glm::normalize( sunDir ), 0.0f );
    }

    // Env eye follows primary fly camera (not per-view scene cameras).

    env.myViewWorldPos = glm::vec4( aCore.myPrimaryCamera.myEye, 1.0f );

    char* mapGpuEnvData = nullptr;

    vmaMapMemory( aCore.myRhi.myDeviceCtx.myAllocator, aCore.myEnvDataBuffer.myAllocation, ( void** )&mapGpuEnvData );

    mapGpuEnvData += aCore.PadUniformBufferSize( sizeof( Gpu_EnvironmentData ) ) * aCurrentFrame;

    memcpy( mapGpuEnvData, &env, sizeof( Gpu_EnvironmentData ) );

    vmaUnmapMemory( aCore.myRhi.myDeviceCtx.myAllocator, aCore.myEnvDataBuffer.myAllocation );
}

void Vk_FrameUniformUploader::UpdateLightingGlobals( const Vk_Renderer& aCore, uint32_t aCurrentFrame, const glm::mat4& aLightViewProj ) {

    const float prefilterMaxMip = aCore.myIblResourcesState.myInitialized ? aCore.myIblResourcesState.myPrefilterMaxMipLevel : 0.0f;

    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );

    Gpu_LightingGlobals globals = Gfx_BuildLightingGlobals( aCore.myLightingSettings, aLightViewProj, prefilterMaxMip, sunDir, Vk_ShadowMapState::kMapSize );

    char* mapData = nullptr;

    vmaMapMemory( aCore.myRhi.myDeviceCtx.myAllocator, aCore.myLightingGlobalsBuffer.myAllocation, ( void** )&mapData );

    mapData += aCore.PadUniformBufferSize( sizeof( Gpu_LightingGlobals ) ) * aCurrentFrame;

    memcpy( mapData, &globals, sizeof( Gpu_LightingGlobals ) );

    vmaUnmapMemory( aCore.myRhi.myDeviceCtx.myAllocator, aCore.myLightingGlobalsBuffer.myAllocation );
}

void Vk_FrameUniformUploader::UpdateLightingGlobalsFromScene( const Vk_Renderer& aCore, uint32_t aCurrentFrame ) {

    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );

    const Gfx_Bounds sceneBounds = aCore.GetShadowCasterBounds();

    const Gfx_LightingMath::Gfx_DirectionalShadowSetup shadowSetup =
        Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowSetup( sunDir, sceneBounds, Vk_ShadowMapState::kMapSize );

    UpdateLightingGlobals( aCore, aCurrentFrame, shadowSetup.myLightViewProj );
}
