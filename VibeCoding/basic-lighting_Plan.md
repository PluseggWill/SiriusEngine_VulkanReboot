# Plan: basic-lighting

## Problem statement and goals

Add a **basic lighting model** (ambient + Lambert diffuse) to the active HLSL path (`TriangleShader.hlsl`), using the existing `GpuEnvironmentData` UBO (binding 1). The mesh must supply per-vertex normals so `N·L` is meaningful on 3D geometry.

## Non-goals

- Blinn-Phong / PBR / shadow maps
- Texture sampling / material system changes
- Fog implementation (fields exist in UBO but unused)
- Removing legacy GLSL sources in this task

## Design decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Lighting model | `ambient + diffuse` (Lambert) | Standard “basic” baseline |
| Normals | Add `Vertex::normal`, compute on load if OBJ lacks `vn` | Required for diffuse on arbitrary meshes |
| Normal space | World space in VS/PS | Matches directional light in env UBO |
| Albedo | Vertex `color` | Already in layout; no sampler work |
| Light data | `GpuEnvironmentData` binding 1 | Descriptor layout already wired |
| C++ UBO | Stable sunlight dir/color; subdued ambient | Current code animates ambient and leaves light at `(1,1,1)` |

**Light direction convention:** `sunlightDirection.xyz` = normalized direction **from surface toward the sun** (shader uses `L = normalize(sunlightDirection.xyz)`).

## File touch list

- `VulkanDesktop/Shader/TriangleShader.hlsl`
- `VulkanDesktop/Shader/CompileShader.bat` (run only)
- `VulkanDesktop/RenderCore/Vk_Types.h`
- `VulkanDesktop/RenderCore/Vk_Types.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.cpp` (`UpdateUniformBuffer`)

## Implementation steps

- [x] 1. Extend `Vertex` with `normal`; add Vulkan attribute location 3
- [x] 2. Mesh load: read OBJ normals or accumulate face normals per vertex
- [x] 3. HLSL: pass world-space normal; fragment ambient + Lambert using `EnvironmentData` (b1)
- [x] 4. C++: set stable env UBO values (sun direction, colors)
- [x] 5. Compile SPIR-V via `CompileShader.bat` (glslc fallback; DXC not in PATH)
- [x] 6. Manual run: rotating mesh shows shading, no validation spam

## Test / verification

1. Run `VulkanDesktop\Shader\CompileShader.bat` with `USE_DXC=1` if available.
2. Rebuild `VulkanDesktop` (Release).
3. Run exe from output dir; expect lit mesh (not flat magenta), darker on faces away from light.
4. Optional: Vulkan validation layers clean for one frame.

## Risks and rollback

- **Vertex stride change** requires mesh buffer rebuild (happens on load at startup).
- **std140 mismatch** on env UBO → validation errors; keep field order identical to `GpuEnvironmentData`.
- Rollback: revert shader + vertex layout + UBO update.
