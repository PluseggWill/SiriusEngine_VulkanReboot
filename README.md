# VulkanDesktop

Vulkan learning / engine reboot sandbox (SiriusEngine).

## Run (Windows)

Build `VulkanDesktop.sln` (Debug|x64), run `x64\Debug\VulkanDesktop.exe`.

## Shaders (GLSL → SPIR-V)

**Sources** (`VulkanDesktop/Shader/`): `TriangleVertex.vert` + `TriangleFrag_Lit.frag` → SPIR-V via vendored **glslc** (`lib/VulkanSDK/1.2.182.0/Bin32/glslc.exe`).

**Generated** (`VulkanDesktop/Shader_Generated/`): `TriangleVert.spv`, `TrianglePix.spv` — do not edit by hand.

In Visual Studio, `TriangleVertex.vert` is a **Custom Build** item: editing GLSL sources or compile scripts marks the project out of date; the next **Build** / **F5** recompiles SPIR-V into `Shader_Generated/`.

Manual compile from `VulkanDesktop\Shader`:

```bat
CompileShader.bat
```

Logs: `Logs/shader_compile_log.txt`, `Logs/engine_runtime_log.txt`.

## Debug camera

| Input | Action |
|-------|--------|
| W / S / A / D | Move on horizontal plane (view-relative) |
| Q / E | Down / up (world Z) |
| Hold **RMB** + mouse | Look around |
| ImGui **Camera** panel | Move speed, mouse sensitivity |

Details: `VibeCoding/fps-camera_Plan.md`. Roadmap: `VibeCoding/TODOList.md`. Architecture: `VibeCoding/EngineArchitecture.md` §3.1.
