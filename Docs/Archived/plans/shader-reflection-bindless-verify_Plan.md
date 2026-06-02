# Plan: shader-reflection-bindless-verify (2d)

**Sprint:** S2 — Shader systems  
**Status:** Closed (2026-06-01)  
**Prerequisite:** shader-reflection (2a), shader-layout-from-reflection (2b) — done  
**Roadmap:** [`Active-Plan.md`](Active-Plan.md) — *Shader reflection bindless verify (2d)*

---

## Problem

Batch lit path has MSBuild SPIR-V ↔ `DescriptorContract_LitBatch.json` validation. Bindless uses `TrianglePix_Bindless.spv` + hand-written `CreateBindlessMaterialSetLayout` with no offline guard — drift risks silent validation errors at runtime.

## Goals

1. `DescriptorContract_LitBindless.json` for `lit_bindless` (Set 1 texture array + material SSBO; Sets 0/2 shared with vert).
2. `ReflectShaders_LitBindless.bat` → `reflection_lit_bindless.json`; wired after glslc in `CompileShader_Glslc.bat`.
3. MSBuild outputs include bindless SPV + JSON; build fails on contract drift.
4. Runtime: `VerifyLitBindlessReflectionContract()` when bindless path selected (name/type/stage vs `Vk_Enum`).

## Non-goals

- Auto-generate bindless `VkDescriptorSetLayout` from JSON (future 2b extension).
- Permutation registry / pipeline disk cache.
- SPIR-V array `count` vs `kMaxBindlessTextures` strict MSBuild check (runtime array reports count=1).

## Touch list

| Path | Change |
|------|--------|
| `Shader/DescriptorContract_LitBindless.json` | New contract |
| `Shader/ReflectShaders_LitBindless.bat` | New MSBuild hook |
| `Shader/CompileShader_Glslc.bat` | Call bindless reflect |
| `Shader_Generated/reflection_lit_bindless.json` | Generated artifact |
| `VulkanDesktop.vcxproj` | Custom build outputs/inputs |
| `RenderCore/Vk_ShaderEffectMeta.*` | Load + verify bindless JSON |
| `RenderCore/Vk_DescriptorSystem.cpp` | Call verify on bindless init |

## Steps

- [x] P1 — Contract JSON + reflect bat + compile chain
- [x] P2 — vcxproj Custom Build outputs
- [x] P3 — Runtime `VerifyLitBindlessReflectionContract`
- [x] P4 — Build Debug\|x64 + smoke `--no-validation --smoke-seconds 6`

## Verification

```powershell
& "<MSBuild>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
Set-Location x64\Debug
.\VulkanDesktop.exe --no-validation --smoke-seconds 6
```

**Log:** `[SHADER-REFLECT] lit_bindless OK`; if bindless GPU path: `bindless reflection verify OK`.

## Risks

- Machines without descriptor indexing fall back to batch — verify only runs on bindless path.
