# Plan: lighting-plus

## Goals

Extend the lit shader path with:

1. **Blinn-Phong specular** (H·N shininess)
2. **Texture albedo** (`viking_room` sampler at binding 2)
3. **ImGui lighting panel** editing `Vk_Core::myEnvironmentData` at runtime

## Non-goals

- PBR, normal maps, multiple lights, shadows
- Fog implementation

## Design

| Item | Decision |
|------|----------|
| Material params | `fogDistances`: x=specularStrength, y=shininess, z=textureBlend |
| View vector | VS outputs `worldPos`; FS uses `myViewWorldPos` in env UBO (6th `vec4`) |
| Texture | `COMBINED_IMAGE_SAMPLER` binding `eVk_TextureBinding = 2` |
| UI | `LightingPanel_Build(GpuEnvironmentData&)` — ambient/sun/spec/tex blend |
| UBO source | `UpdateUniformBuffer` copies `myEnvironmentData` + camera eye |

## Files

- `VulkanDesktop/RenderCore/Vk_Types.h` — `myViewWorldPos`
- `VulkanDesktop/RenderCore/Vk_Enum.h` — texture binding
- `VulkanDesktop/RenderCore/Vk_Core.cpp` — descriptors, init defaults, UBO upload
- `VulkanDesktop/Shader/TriangleShader.hlsl`, `TriangleVertex.vert`, `TriangleFrag_Lit.frag`
- `VulkanDesktop/Editor/LightingPanel.{h,cpp}` — new
- `VulkanDesktop/VulkanDesktop.vcxproj` — add LightingPanel

## Steps

- [x] Plan
- [x] Env UBO + descriptor binding 2
- [x] Shaders: worldPos, texture albedo, Blinn-Phong
- [x] C++ init + UpdateUniformBuffer from `myEnvironmentData`
- [x] ImGui lighting panel
- [x] Compile shaders (glslc)
- [x] Manual run verification

## Verification

Rebuild, run: texture visible, specular highlight, sliders change lighting live.
