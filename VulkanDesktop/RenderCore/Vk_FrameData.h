#pragma once
#include <array>

#include "../Gfx/Gfx_RenderView.h"
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

// Per in-flight frame (MAX_FRAMES_IN_FLIGHT): sync objects, command buffer, camera UBO, descriptor set.
struct Vk_FrameData {
    VkSemaphore myPresentSemaphore;  // signaled when swapchain image is ready
    VkSemaphore myRenderSemaphore;   // signaled when GPU finishes this frame's cmd buffer
    VkFence     myRenderFence;

    Vk_DeletionQueue myFrameDeletionQueue;

    VkCommandBuffer myCommandBuffer;

    Vk_AllocatedBuffer                                myCameraBuffer;         // sizeof(GpuCameraData) * kGfxMaxRenderViews, one slab per frame
    std::array< VkDescriptorSet, kGfxMaxRenderViews > myGlobalDescriptors{};  // set 0 (Frame): camera + env

    // Per-frame instance ring UBO (GpuObjectData slices); persistently CPU-mapped for FillInstanceSlab.
    Vk_AllocatedBuffer myObjectBuffer;
    void*              myInstanceSlabMapped = nullptr;
    VkDescriptorSet    myObjectDescriptor;  // set 2 | UNIFORM_BUFFER_DYNAMIC | instance slab

    // M2 prep §A: CPU-filled VkDrawIndexedIndirectCommand[] + Gfx_DrawTemplate[] (GPU cull will consume same layout).
    // Partitioned per view via myDrawBufferBaseIndex in Gfx_FrameRenderPacket (see Vk_FrameDrawPrep).
    Vk_AllocatedBuffer myDrawIndirectBuffer;
    void*              myDrawIndirectMapped = nullptr;
    Vk_AllocatedBuffer myDrawTemplateBuffer;
    void*              myDrawTemplateMapped = nullptr;

    // P3: per SoA slot Gfx_EntityGpuRecord[] (scene-wide; filled before multi-view draw prep).
    Vk_AllocatedBuffer myEntityRecordBuffer;
    void*              myEntityRecordMapped = nullptr;
};