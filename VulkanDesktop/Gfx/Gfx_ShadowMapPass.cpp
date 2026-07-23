#include "Gfx_ShadowMapPass.h"

namespace {

struct ShadowPushConstants {
    alignas( 16 ) glm::mat4 myLightViewProj;
};
static_assert( sizeof( ShadowPushConstants ) == 64, "ShadowPushConstants must match ShadowMap.vert push block" );

}  // namespace

namespace Gfx_ShadowMapPass {

bool CreateResources( Rhi_Device& aDevice, const ResourcesInitDesc& aDesc, PassState& aState ) {
    if ( aState.myResourcesReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myDepthFormat == Rhi_Format::Undefined || !aDesc.myObjectSetLayout || aDesc.myVertSpirv == nullptr
         || aDesc.myVertSpirvBytes == 0 || aDesc.myFragSpirv == nullptr || aDesc.myFragSpirvBytes == 0 ) {
        return false;
    }

    Rhi::TextureDesc texDesc{};
    texDesc.myWidth  = kMapSize;
    texDesc.myHeight = kMapSize;
    texDesc.myFormat = aDesc.myDepthFormat;
    texDesc.myUsage  = Rhi_ImageUsage::DepthStencilAttachment | Rhi_ImageUsage::Sampled;
    texDesc.myMemory = Rhi_MemoryUsage::GpuOnly;
    aState.myDepth   = Rhi::DeviceCreateTexture( aDevice, texDesc );
    if ( !aState.myDepth ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    Rhi::AttachmentDesc depthAttachment{};
    depthAttachment.myFormat        = aDesc.myDepthFormat;
    depthAttachment.myLoadOp        = Rhi_AttachmentLoadOp::Clear;
    depthAttachment.myStoreOp       = Rhi_AttachmentStoreOp::Store;
    depthAttachment.myInitialLayout = Rhi_ImageLayout::DepthStencilAttachment;
    depthAttachment.myFinalLayout   = Rhi_ImageLayout::DepthStencilReadOnly;
    depthAttachment.mySampleCount   = 1;

    Rhi::RenderPassDesc rpDesc{};
    rpDesc.myAttachments                 = &depthAttachment;
    rpDesc.myAttachmentCount             = 1;
    rpDesc.myColorAttachmentCount        = 0;
    rpDesc.myHasDepthStencil             = true;
    rpDesc.myDepthStencilAttachmentIndex = 0;
    aState.myRenderPass                  = Rhi::DeviceCreateRenderPass( aDevice, rpDesc );
    if ( !aState.myRenderPass ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    const Rhi_Texture    fbAttachments[] = { aState.myDepth };
    Rhi::FramebufferDesc fbDesc{};
    fbDesc.myRenderPass      = aState.myRenderPass;
    fbDesc.myAttachments     = fbAttachments;
    fbDesc.myAttachmentCount = 1;
    fbDesc.myWidth           = kMapSize;
    fbDesc.myHeight          = kMapSize;
    aState.myFramebuffer     = Rhi::DeviceCreateFramebuffer( aDevice, fbDesc );
    if ( !aState.myFramebuffer ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Vertex;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = static_cast< uint32_t >( sizeof( ShadowPushConstants ) );

    Rhi::PipelineLayoutDesc layoutDesc{};
    layoutDesc.mySetLayouts     = &aDesc.myObjectSetLayout;
    layoutDesc.mySetLayoutCount = 1;
    layoutDesc.myPushRanges     = &push;
    layoutDesc.myPushRangeCount = 1;
    aState.myLayout             = Rhi::DeviceCreatePipelineLayout( aDevice, layoutDesc );
    if ( !aState.myLayout ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    Rhi::SamplerDesc compareSampler{};
    compareSampler.myMagFilter     = Rhi_Filter::Linear;
    compareSampler.myMinFilter     = Rhi_Filter::Linear;
    compareSampler.myAddressU      = Rhi_AddressMode::ClampToBorder;
    compareSampler.myAddressV      = Rhi_AddressMode::ClampToBorder;
    compareSampler.myAddressW      = Rhi_AddressMode::ClampToBorder;
    compareSampler.myCompareEnable = true;
    compareSampler.myCompareOp     = Rhi_CompareOp::GreaterOrEqual;
    aState.myCompareSampler        = Rhi::DeviceCreateSampler( aDevice, compareSampler );
    if ( !aState.myCompareSampler ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    Rhi::SamplerDesc readSampler{};
    readSampler.myMagFilter   = Rhi_Filter::Nearest;
    readSampler.myMinFilter   = Rhi_Filter::Nearest;
    readSampler.myAddressU    = Rhi_AddressMode::ClampToBorder;
    readSampler.myAddressV    = Rhi_AddressMode::ClampToBorder;
    readSampler.myAddressW    = Rhi_AddressMode::ClampToBorder;
    aState.myDepthReadSampler = Rhi::DeviceCreateSampler( aDevice, readSampler );
    if ( !aState.myDepthReadSampler ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    Rhi_ShaderModule vert = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myVertSpirv, aDesc.myVertSpirvBytes );
    Rhi_ShaderModule frag = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myFragSpirv, aDesc.myFragSpirvBytes );
    if ( !vert || !frag ) {
        Rhi::DeviceDestroyShaderModule( aDevice, vert );
        Rhi::DeviceDestroyShaderModule( aDevice, frag );
        DestroyResources( aDevice, aState );
        return false;
    }

    Rhi::GraphicsPipelineDesc pipeDesc{};
    pipeDesc.myVertexShader           = vert;
    pipeDesc.myFragmentShader         = frag;
    pipeDesc.myLayout                 = aState.myLayout;
    pipeDesc.myRenderPass             = aState.myRenderPass;
    pipeDesc.myColorAttachmentCount   = 0;
    pipeDesc.mySampleCount            = 1;
    pipeDesc.myCullMode               = Rhi_CullMode::Back;
    pipeDesc.myDepthTestEnable        = true;
    pipeDesc.myDepthWriteEnable       = true;
    pipeDesc.myDepthCompareOp         = Rhi_CompareOp::Greater;
    pipeDesc.myBlendEnable            = false;
    pipeDesc.myDynamicViewportScissor = true;
    pipeDesc.myDynamicDepthBias       = true;
    pipeDesc.myStandardGfxVertexInput = true;

    aState.myPipeline = Rhi::DeviceCreateGraphicsPipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, vert );
    Rhi::DeviceDestroyShaderModule( aDevice, frag );
    if ( !aState.myPipeline ) {
        DestroyResources( aDevice, aState );
        return false;
    }

    aState.myResourcesReady = true;
    return true;
}

void DestroyResources( Rhi_Device& aDevice, PassState& aState ) {
    if ( aState.myPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myPipeline );
    }
    if ( aState.myLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myLayout );
    }
    if ( aState.myCompareSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myCompareSampler );
    }
    if ( aState.myDepthReadSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myDepthReadSampler );
    }
    if ( aState.myFramebuffer ) {
        Rhi::DeviceDestroyFramebuffer( aDevice, aState.myFramebuffer );
    }
    if ( aState.myRenderPass ) {
        Rhi::DeviceDestroyRenderPass( aDevice, aState.myRenderPass );
    }
    if ( aState.myDepth ) {
        Rhi::DeviceDestroyTexture( aDevice, aState.myDepth );
    }
    aState.myResourcesReady = false;
}

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    const Rhi::ClearValue clear = Rhi::MakeClearDepthStencil( 0.0f, 0 );

    Rhi::RenderPassBeginInfo begin{};
    begin.myRenderPass  = aGpu.myRenderPass;
    begin.myFramebuffer = aGpu.myFramebuffer;
    begin.myWidth       = kMapSize;
    begin.myHeight      = kMapSize;
    begin.myClears      = &clear;
    begin.myClearCount  = 1;

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=ShadowMap" );
    }

    Rhi::CommandListBeginRenderPass( aCmd, begin );
    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myPipeline );
    Rhi::CommandListSetViewport( aCmd, Rhi::Viewport{ 0.f, 0.f, static_cast< float >( kMapSize ), static_cast< float >( kMapSize ), 0.f, 1.f } );
    Rhi::CommandListSetScissor( aCmd, Rhi::Scissor{ 0, 0, kMapSize, kMapSize } );
    Rhi::CommandListSetDepthBias( aCmd, aInput.myDepthBiasConstant, 0.0f, aInput.myDepthBiasSlope );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Vertex, 0, sizeof( aInput.myLightViewProj ), &aInput.myLightViewProj );

    for ( uint32_t i = 0; i < aInput.myDrawCount; ++i ) {
        const DrawItem& draw = aInput.myDraws[ i ];
        Rhi::CommandListBindVertexBuffer( aCmd, 0, draw.myVertexBuffer, 0 );
        Rhi::CommandListBindIndexBuffer( aCmd, draw.myIndexBuffer, 0, Rhi_IndexType::Uint32 );
        Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myLayout, 0, aGpu.myObjectSet, &draw.myDynamicOffset, 1 );
        if ( aInput.myDrawCallsOut != nullptr ) {
            ( *aInput.myDrawCallsOut )++;
        }
        Rhi::CommandListDrawIndexed( aCmd, draw.myIndexCount, 1, 0, 0, 0 );
    }

    Rhi::CommandListEndRenderPass( aCmd );

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }
}

}  // namespace Gfx_ShadowMapPass
