#pragma once

#include "../Util/Util_EngineConfig.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct Vk_DeletionQueue;
class Vk_Core;

// ShaderEffectMeta: lit_batch descriptor layout from reflection_lit.json (S2 phase 2b).
// SPIR-V reports UNIFORM_BUFFER for Set 2; engine applies UNIFORM_BUFFER_DYNAMIC at layout create.
struct ShaderResource {
    uint32_t           myBinding    = 0;
    VkDescriptorType   myType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uint32_t           myCount      = 1;
    VkShaderStageFlags myStageFlags = 0;
    std::string        myName;
};

struct DescriptorSetLayoutData {
    uint32_t                                       mySetNumber = 0;
    std::unordered_map< uint32_t, ShaderResource > myBindings;
};

struct ShaderEffectMeta {
    std::string                                             myPipelineGroup;
    std::unordered_map< uint32_t, DescriptorSetLayoutData > mySets;
};

// Handles for lit_batch pipeline layout order: Set 0 frame, Set 1 material, Set 2 object (dynamic UBO).
struct LitBatchDescriptorSetLayouts {
    VkDescriptorSetLayout myGlobalSetLayout   = VK_NULL_HANDLE;
    VkDescriptorSetLayout myMaterialSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myObjectSetLayout   = VK_NULL_HANDLE;
};

namespace VkShaderEffectMeta {

// Same path as MSBuild ShaderReflect output (2a); resolved via UtilLoader::ResolvePath.
inline constexpr const char* kLitBatchReflectionLogicalPath = "VulkanDesktop/Shader_Generated/reflection_lit.json";
// S2 phase 2d: bindless frag SPIR-V contract (Set 1 texture array + material SSBO). MSBuild validates; runtime checks types when Bindless path is active.
inline constexpr const char* kLitBindlessReflectionLogicalPath = "VulkanDesktop/Shader_Generated/reflection_lit_bindless.json";

ShaderEffectMeta LoadLitBatchFromReflectionJson( const Util_EngineConfig& aConfig, const std::string& aLogicalPath = kLitBatchReflectionLogicalPath );
ShaderEffectMeta LoadLitBindlessFromReflectionJson( const Util_EngineConfig& aConfig, const std::string& aLogicalPath = kLitBindlessReflectionLogicalPath );
void             ApplyLitBatchLayoutOverrides( ShaderEffectMeta& aMeta );
size_t           HashLayout( const ShaderEffectMeta& aMeta );
void             LogMetaDump( const ShaderEffectMeta& aMeta );

// Creates Set 0/1/2 layouts via hash cache; registers destroy on aDeletionQueue.
LitBatchDescriptorSetLayouts AcquireLitBatchDescriptorSetLayouts( Vk_Core& aCore );

// S2 phase 2d: compare reflection_lit_bindless.json Set 1 vs hand-written bindless layout (Vk_Enum / Vk_DescriptorPolicy).
void VerifyLitBindlessReflectionContract( const Util_EngineConfig& aConfig );

}  // namespace VkShaderEffectMeta

struct Vk_DeviceContext;
struct Vk_SceneGpuContext;
class Vk_Core;

void VkShaderEffectMeta_RunLitBatchLayoutMismatchValidationTest( Vk_DeviceContext& aDevice, Vk_SceneGpuContext& aScene, Vk_Core& aCoreOps );
