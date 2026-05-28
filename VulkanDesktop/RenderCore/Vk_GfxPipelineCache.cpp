#include "Vk_GfxPipelineCache.h"

#include "Vk_Core.h"

void Vk_GfxPipelineCache::InitScenePipelines( Vk_Core& aCore ) {
    aCore.CreateGfxPipeline();
    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        aCore.CreateBindlessGfxPipelines();
    }
}
