# Plan: forward-stage1-contracts

**Sprint:** S2 — Stage 1 forward gate (epic A)  
**Status:** Closed (2026-06-01)  
**Roadmap:** [`Active-Plan.md`](Active-Plan.md) — *Stage 1 gate: forward baseline contracts*  
**Epic:** [`forward-rendering-epic_Plan.md`](forward-rendering-epic_Plan.md) § A

## Goals

1. **Material UBO / bindless table** — freeze std140 `GpuMaterialParams` and std430 `GpuMaterialTableEntry` with PBR-ready fields (shader still uses Blinn-Phong + alpha; roughness/metallic reserved).
2. **Scene JSON** — optional `baseColor`, `roughness`, `metallic`, `alphaMode`; propagate manifest → GPU upload.
3. **Render preset** — `ForwardLit` / `ForwardLitAlphaClip` map to permutation registry names; `engine.json` + `--render-preset`; `shaderPermutation` / CLI wins when set.
4. **Feature bits** — document `PBR` reserved in `Gfx_ShaderFeatureBit` + registry comment; transparency sort contract in Architecture + SceneJSON.
5. **Non-goals:** PBR shading in frag, `HybridDeferred` preset implementation, golden captures (epic C).

## Touch list

| Area | Files |
|------|--------|
| GPU structs / shader | `Vk_Types.h`, `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag` |
| Scene | `Gfx_SceneDesc.h`, `Gfx_SceneLoader.cpp`, `Gfx_ResourceManifest.h`, `Gfx_SceneApply.cpp` |
| Upload | `Vk_ResourceTables.*`, `Vk_DescriptorSystem.cpp` |
| Preset | `Gfx_RenderPreset.*`, `Util_EngineConfig.*`, `Config/engine.json`, `Docs/CLI.md` |
| Docs | `EngineArchitecture.md`, `SceneJSON.md`, `forward-rendering-epic_Plan.md` |

## Steps

1. [ ] Extend material structs + GLSL layouts (batch + bindless).
2. [ ] Scene parse + manifest + resource table + descriptor upload.
3. [ ] `Gfx_RenderPreset` + config/CLI resolution + default `ForwardLit` in `engine.json`.
4. [ ] Architecture / SceneJSON / epic A checkboxes + CLI help.
5. [ ] Build Debug\|x64 + `--no-validation --smoke-seconds 6`.

## Verification

- MSBuild `VulkanDesktop.sln` Debug\|x64 exit 0.
- `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0.
- Log: `[CONFIG] renderPreset=ForwardLit`, `[SHADER-PERM] active permutation name=lit`, `[SCENE] LoadSceneResources completed`.
