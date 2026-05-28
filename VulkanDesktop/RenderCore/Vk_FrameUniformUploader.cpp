#include "Vk_FrameUniformUploader.h"

#include "Vk_Core.h"

void Vk_FrameUniformUploader::Update( const Vk_Core& aCore, uint32_t aCurrentFrame ) {
    aCore.UpdateUniformBuffer( aCurrentFrame );
}
