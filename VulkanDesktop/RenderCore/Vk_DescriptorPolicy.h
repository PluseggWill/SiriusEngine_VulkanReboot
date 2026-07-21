#pragma once

#include <cstdint>

// Descriptor binding policy (locked S0) - full rationale: Docs/EngineArchitecture.md section 5.3,
// Docs/descriptor-strategy_Plan.md.
//
// Hybrid by update frequency (not "pick static OR dynamic"):
//   Set 0 Frame   - UNIFORM_BUFFER, one set per MAX_FRAMES_IN_FLIGHT (camera, env, ...).
//   Set 1 Material - UNIFORM_BUFFER / samplers per batch (S1).
//   Set 2 Object   - UNIFORM_BUFFER_DYNAMIC into a per-frame instance slab, and/or push constants (S1).
//
// Push constants: mat4 model (64 B) only for per-draw transform unless maxPushConstantsSize allows more.
// Do not put full Gpu_CameraData (272 B) in push constants without a capability check (min is often 128 B).

namespace VkDescriptorPolicy {

// Pipeline set indices - must match VkPipelineLayoutCreateInfo::pSetLayouts order when multi-set is wired (S1).
constexpr uint32_t kSetFrame    = 0;
constexpr uint32_t kSetMaterial = 1;  // S1
constexpr uint32_t kSetObject   = 2;  // S1

// Set 2 UNIFORM_BUFFER_DYNAMIC path wired in RecordScenePass (S1 verify 2026-05-26).
inline constexpr bool kUseDynamicUniformForInstanceSlab = true;

// Max draws written into the per-frame instance ring UBO (FillInstanceSlab); no per-draw heap alloc.
inline constexpr uint32_t kMaxInstanceSlabEntries = 256;

// Draw-template / indirect buffers share instance-slab partition sizing (PrepareFrameCpu splits by active view count).
inline constexpr uint32_t kMaxDrawTemplateEntries = kMaxInstanceSlabEntries;

// Entity-record SSBO (per SoA slot, scene-wide — P3 GPU cull input).
inline constexpr uint32_t kMaxEntitySlots = kMaxInstanceSlabEntries;

// Bindless Set 1 texture array capacity (S1 bindless v0).
// CONTRACT: TriangleFrag_Lit_Bindless.frag VK_MAX_BINDLESS_TEXTURES + layout descriptorCount must match.
inline constexpr uint32_t kMaxBindlessTextures = 64;

// Scene-load descriptor pool policy max (fail LoadScene before vkCreateDescriptorPool).
inline constexpr uint32_t kMaxSceneMaterials = 512;
inline constexpr uint32_t kMaxSceneTextures  = 512;

// --- Material / descriptor rebuild (S2 layout verify) ---
// Device boot: Vk_DescriptorSystem::InitDeviceLayouts -> CreateDescriptorSetLayout (+ bindless set layout if enabled).
// LoadScene: InitSceneDescriptors -> pool, per-frame Set 0/2 descriptors, then CreateMaterialDescriptorSets (batch)
//   or CreateBindlessDescriptorResources (bindless). Gfx_Material stores pipeline + layout handles from manifest load.
// Swapchain recreate: RefreshMaterialPipelinesAfterSwapchainRecreate updates Gfx_Material pipeline handles only.
// Material count, texture views, or bindless table content change: UnloadSceneGpuResources then LoadSceneGpuResources (full GPU teardown).

}  // namespace VkDescriptorPolicy
