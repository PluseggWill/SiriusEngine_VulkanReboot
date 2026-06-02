# Plan: permutation-registry

**Sprint:** S2 — Shader systems  
**Status:** Closed (2026-06-01)  
**Roadmap:** [`Active-Plan.md`](../../Active-Plan.md)

## Goals

1. `Shader/PermutationRegistry.json` — feature names + SPIR-V paths.
2. `Gfx_ShaderPermutation` — load registry; active perm from `engine.json` / `--shader-permutation`.
3. Offline `TrianglePix_AlphaClip.spv` (`-DALPHA_CLIP=1`) as second variant.
4. Extract sets `myPipelinePermutationId` + sort-key perm slot encoding.
5. `LoadSceneResources` uses active perm SPIR-V paths for batch pipelines.

## Non-goals

- Shadow/IBL permutations (bits reserved only).
- Per-material perm from scene JSON (manifest field stays 0).
- `VkPipelineCache` disk blob (separate Active-Plan task).

## Verification

MSBuild Debug|x64; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6`; log `[SHADER-PERM] active permutation`.
