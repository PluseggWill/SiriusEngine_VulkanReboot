# Plan: flat-world-matrices

**Sprint:** S2 — Engine layering and hygiene (scene/data-plane follow-up)  
**Status:** Closed (2026-06-02)  
**Task line:** moved from `Docs/Active-Plan.md` to `Docs/Archived-Plan.md` (`[S2]`)  
**Related docs:** [`../../EngineArchitecture.md`](../../EngineArchitecture.md) §4.5, §5.4; [`../../SceneJSON.md`](../../SceneJSON.md) §3.8.

## Approved option set

- **1B + 2A + 3A + 4A + 5A/5B + 6B**

## Scope

- Keep v1 scene JSON semantics unchanged (`entities[].transform` stays flat world input).
- Move source/resolved transform ownership to Gfx-side state.
- Add explicit resolve stage before render extract path.
- Rename SoA transform API to world-explicit naming.
- Preserve runtime behavior including demo rotate.

## Delivered design

1. **Gfx-side transform state:** `Gfx_SceneTransformState` holds source/resolved world arrays.
2. **Simulation + resolve split:** `Gfx_TickDemoSceneTransforms` writes resolved state; `Gfx_ResolveFlatWorldTransforms` publishes to SoA world transforms.
3. **Application order:** `Input/Camera -> TickDemo -> Resolve -> Render`.
4. **World-explicit API:** `GetWorldTransform` / `SetWorldTransform` consumed by extract/cull/slab.
5. **RenderCore ownership cleanup:** removed `Vk_Core::myDemoBaseTransforms`.

## File touch summary

- `VulkanDesktop/Gfx/Gfx_SceneTransform.*` (new)
- `VulkanDesktop/Gfx/Gfx_DemoSceneSim.*`
- `VulkanDesktop/Gfx/Gfx_SceneApply.*`
- `VulkanDesktop/Gfx/Gfx_SceneSoA.*`
- `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp`
- `VulkanDesktop/Gfx/Gfx_DrawCullSort.cpp`
- `VulkanDesktop/App/Application.cpp`
- `VulkanDesktop/RenderCore/Vk_Core.*`
- `VulkanDesktop/RenderCore/Vk_SceneHost.cpp`
- `VulkanDesktop/RenderCore/Vk_FrameDrawPrep.cpp`
- `VulkanDesktop/VulkanDesktop.vcxproj` + `.filters`
- `Docs/EngineArchitecture.md`

## Verification plan and result

- Build: `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` -> exit `0`
- Smoke: `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` -> exit `0`
- Smoke+rotate: `.\VulkanDesktop.exe --no-validation --smoke-seconds 6 --demo-rotate` -> exit `0`

Signals checked: scene load complete, instance slab filled, smoke dwell reached, scene unload complete.
