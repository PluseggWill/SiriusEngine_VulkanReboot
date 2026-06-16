#include "Vk_FgResourceRegistry.h"

bool Vk_FgResourceRegistry::IsDepth( Vk_FgResourceId aId ) {
    return aId == Vk_FgResourceId::GBufferDepth || aId == Vk_FgResourceId::SwapchainDepth;
}
