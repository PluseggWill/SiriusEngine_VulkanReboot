# Notes — 2026-05-22 shader / black screen session

Session outcome: **scene renders correctly with glslc**; HLSL/dxc path removed from the repo afterward.

## Problems encountered

| Issue | Cause | Fix |
|-------|--------|-----|
| Black screen, ImGui OK, draw call = 1 | HLSL/dxc SPIR-V + glm UBO layout mismatch; wrong pipeline entry (`VSMain` vs `main`) | **glslc** build + runtime `"main"` only |
| `engine_runtime_log.txt` not in repo `Logs/` | Relative `Logs/` resolved from VS cwd | `UtilLogger` resolves repo-root `Logs/` |
| MSB8066 / corrupted `TriangleVert.spv` | Custom Build captured bat **stdout** into first Output | Log via **stderr** (`1>&2`) |
| Validation: MSAA resolve on single sample | Single sample with resolve attachment | Direct-to-swapchain when `myMSAASamples == 1` |

## Final toolchain policy

- **Build**: `CompileShader_Glslc.bat` (VS Custom Build on `TriangleVertex.vert`).
- **Runtime**: `TriangleVert.spv` + `TrianglePix.spv`, entry `"main"`.

## Artifacts

- `.cursor/rules/shader-build.mdc` — glslc + Custom Build guidance.
