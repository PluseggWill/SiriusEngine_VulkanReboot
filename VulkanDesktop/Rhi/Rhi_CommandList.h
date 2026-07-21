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

void CommandListBindPipeline( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_Pipeline aPipeline );

void CommandListBindDescriptorSet( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_PipelineLayout aLayout, uint32_t aSetIndex, Rhi_DescriptorSet aSet );

void CommandListPushConstants( Rhi_CommandList& aList, Rhi_PipelineLayout aLayout, Rhi_ShaderStage aStages, uint32_t aOffsetBytes, uint32_t aSizeBytes, const void* aData );

void CommandListDispatch( Rhi_CommandList& aList, uint32_t aGroupCountX, uint32_t aGroupCountY, uint32_t aGroupCountZ );

void CommandListDraw( Rhi_CommandList& aList, uint32_t aVertexCount, uint32_t aInstanceCount, uint32_t aFirstVertex, uint32_t aFirstInstance );

void CommandListDrawIndexed( Rhi_CommandList& aList, uint32_t aIndexCount, uint32_t aInstanceCount, uint32_t aFirstIndex, int32_t aVertexOffset, uint32_t aFirstInstance );

}  // namespace Rhi
