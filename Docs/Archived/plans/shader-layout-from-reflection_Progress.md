# Progress: shader-layout-from-reflection

## Closeout — 2026-06-01

- **Outcome:** Lit batch Set 0/1/2 created from `reflection_lit.json` via `ShaderEffectMeta` + FNV layout cache; Set 2 `UNIFORM_BUFFER` → `UNIFORM_BUFFER_DYNAMIC` engine override. M4: `--descriptor-layout-mismatch-test` probes `vkUpdateDescriptorSets` type mismatch when `VK_LAYER_KHRONOS_validation` + debug messenger are active.
- **Verification:** MSBuild Debug\|x64 exit 0; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0 (`LoadSceneResources completed`, `Smoke dwell reached`, `UnloadScene`). M4 without SDK: exit 1 with `VK_LAYER_KHRONOS_validation not available` (expected); with SDK: `--validation --descriptor-layout-mismatch-test --smoke-frames 2` → `[DESCRIPTOR] layout mismatch test OK` + `[VULKAN-VALIDATION]` VUID.
- **Deviations:** M2 side-by-side legacy layout compare deferred; smoke parity sufficient for v1.
- **Plan:** [`shader-layout-from-reflection_Plan.md`](shader-layout-from-reflection_Plan.md)
