# Progress: basic-lighting

## 2026-05-19 16:51 — Steps 1–5: vertex normals + Lambert shading

- **Plan ref:** Steps 1–5 (vertex layout, mesh normals, HLSL/GLSL, env UBO, shader compile)
- **Files:** `VulkanDesktop/RenderCore/Vk_Types.h`, `VulkanDesktop/RenderCore/Vk_Types.cpp`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `VulkanDesktop/Shader/TriangleShader.hlsl`, `VulkanDesktop/Shader/TriangleVertex.vert`, `VulkanDesktop/Shader/TriangleFrag_Lit.frag`, `VulkanDesktop/Shader/TriangleVert.spv`, `VulkanDesktop/Shader/TrianglePix.spv`
- **What changed:** Added per-vertex normals (OBJ `vn` or smooth-computed). VS outputs world-space normal; PS applies `ambient + diffuse` (Lambert) from `GpuEnvironmentData` (binding 1). C++ env UBO uses stable sun direction/color instead of animated ambient. GLSL fallbacks updated to match HLSL (DXC not on PATH).
- **Verification:** `CompileShader.bat` succeeded (glslc). C++ rebuild not run here (`msbuild` unavailable in agent shell); user should rebuild `VulkanDesktop` in VS and run.
