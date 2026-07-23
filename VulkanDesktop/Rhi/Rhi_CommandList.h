#pragma once

#include "Rhi_Enums.h"
#include "Rhi_Handles.h"

#include <cstdint>

// Module: Rhi_CommandList — Gfx-facing recording surface.

namespace Rhi {

[[nodiscard]] bool CommandListIsValid( const Rhi_CommandList& aList );

// True after backend has bound a native command buffer for recording.
[[nodiscard]] bool CommandListIsRecordingReady( const Rhi_CommandList& aList );

void CommandListBeginDebugLabel( Rhi_CommandList& aList, const char* aName );
void CommandListEndDebugLabel( Rhi_CommandList& aList );

struct ImageBarrier {
    Rhi_Texture       myTexture{};
    Rhi_ImageLayout   myOldLayout = Rhi_ImageLayout::Undefined;
    Rhi_ImageLayout   myNewLayout = Rhi_ImageLayout::General;
    Rhi_PipelineStage mySrcStage  = Rhi_PipelineStage::TopOfPipe;
    Rhi_PipelineStage myDstStage  = Rhi_PipelineStage::BottomOfPipe;
    Rhi_Access        mySrcAccess = Rhi_Access::None;
    Rhi_Access        myDstAccess = Rhi_Access::None;
    uint32_t          myBaseMip   = 0;
    uint32_t          myMipCount  = 1;
};

void CommandListPipelineBarrier( Rhi_CommandList& aList, const ImageBarrier* aBarriers, uint32_t aBarrierCount );

struct BufferBarrier {
    Rhi_Buffer        myBuffer{};
    Rhi_PipelineStage mySrcStage  = Rhi_PipelineStage::TopOfPipe;
    Rhi_PipelineStage myDstStage  = Rhi_PipelineStage::BottomOfPipe;
    Rhi_Access        mySrcAccess = Rhi_Access::None;
    Rhi_Access        myDstAccess = Rhi_Access::None;
};

void CommandListPipelineBarrier( Rhi_CommandList& aList, const BufferBarrier* aBarriers, uint32_t aBarrierCount );

struct MemoryBarrierDesc {
    Rhi_PipelineStage mySrcStage  = Rhi_PipelineStage::TopOfPipe;
    Rhi_PipelineStage myDstStage  = Rhi_PipelineStage::BottomOfPipe;
    Rhi_Access        mySrcAccess = Rhi_Access::None;
    Rhi_Access        myDstAccess = Rhi_Access::None;
};

void CommandListMemoryBarrier( Rhi_CommandList& aList, const MemoryBarrierDesc& aBarrier );

void CommandListBindPipeline( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_Pipeline aPipeline );

void CommandListBindDescriptorSet( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_PipelineLayout aLayout, uint32_t aSetIndex, Rhi_DescriptorSet aSet,
                                   const uint32_t* aDynamicOffsets = nullptr, uint32_t aDynamicOffsetCount = 0 );

void CommandListPushConstants( Rhi_CommandList& aList, Rhi_PipelineLayout aLayout, Rhi_ShaderStage aStages, uint32_t aOffsetBytes, uint32_t aSizeBytes, const void* aData );

void CommandListDispatch( Rhi_CommandList& aList, uint32_t aGroupCountX, uint32_t aGroupCountY, uint32_t aGroupCountZ );

void CommandListDraw( Rhi_CommandList& aList, uint32_t aVertexCount, uint32_t aInstanceCount, uint32_t aFirstVertex, uint32_t aFirstInstance );

void CommandListDrawIndexed( Rhi_CommandList& aList, uint32_t aIndexCount, uint32_t aInstanceCount, uint32_t aFirstIndex, int32_t aVertexOffset, uint32_t aFirstInstance );

void CommandListBindVertexBuffer( Rhi_CommandList& aList, uint32_t aBinding, Rhi_Buffer aBuffer, uint64_t aOffsetBytes = 0 );

void CommandListBindIndexBuffer( Rhi_CommandList& aList, Rhi_Buffer aBuffer, uint64_t aOffsetBytes, Rhi_IndexType aIndexType );

struct Viewport {
    float myX        = 0.0f;
    float myY        = 0.0f;
    float myWidth    = 0.0f;
    float myHeight   = 0.0f;
    float myMinDepth = 0.0f;
    float myMaxDepth = 1.0f;
};

struct Scissor {
    int32_t  myX      = 0;
    int32_t  myY      = 0;
    uint32_t myWidth  = 0;
    uint32_t myHeight = 0;
};

void CommandListSetViewport( Rhi_CommandList& aList, const Viewport& aViewport );
void CommandListSetScissor( Rhi_CommandList& aList, const Scissor& aScissor );
void CommandListSetDepthBias( Rhi_CommandList& aList, float aConstantFactor, float aClamp, float aSlopeFactor );

struct ClearValue {
    Rhi_ClearValueType myType = Rhi_ClearValueType::Color;
    float              myColor[ 4 ]{ 0.0f, 0.0f, 0.0f, 1.0f };
    float              myDepth   = 1.0f;
    uint32_t           myStencil = 0;
};

[[nodiscard]] inline ClearValue MakeClearColor( float aR, float aG, float aB, float aA ) {
    ClearValue value{};
    value.myType       = Rhi_ClearValueType::Color;
    value.myColor[ 0 ] = aR;
    value.myColor[ 1 ] = aG;
    value.myColor[ 2 ] = aB;
    value.myColor[ 3 ] = aA;
    return value;
}

[[nodiscard]] inline ClearValue MakeClearDepthStencil( float aDepth = 1.0f, uint32_t aStencil = 0 ) {
    ClearValue value{};
    value.myType    = Rhi_ClearValueType::DepthStencil;
    value.myDepth   = aDepth;
    value.myStencil = aStencil;
    return value;
}

struct RenderPassBeginInfo {
    Rhi_RenderPass    myRenderPass{};
    Rhi_Framebuffer   myFramebuffer{};
    int32_t           myOffsetX    = 0;
    int32_t           myOffsetY    = 0;
    uint32_t          myWidth      = 0;
    uint32_t          myHeight     = 0;
    const ClearValue* myClears     = nullptr;
    uint32_t          myClearCount = 0;
};

void CommandListBeginRenderPass( Rhi_CommandList& aList, const RenderPassBeginInfo& aInfo );
void CommandListEndRenderPass( Rhi_CommandList& aList );

struct ImageCopy {
    Rhi_Texture     mySrc{};
    Rhi_Texture     myDst{};
    Rhi_ImageLayout mySrcLayout = Rhi_ImageLayout::TransferSrc;
    Rhi_ImageLayout myDstLayout = Rhi_ImageLayout::TransferDst;
    uint32_t        myWidth     = 1;
    uint32_t        myHeight    = 1;
};

void CommandListCopyImage( Rhi_CommandList& aList, const ImageCopy& aCopy );

}  // namespace Rhi
