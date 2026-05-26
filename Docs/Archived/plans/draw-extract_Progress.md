# draw-extract — Progress

## 2026-05-25 — Plan created

- **Plan ref:** Initial plan from SprintPlan S1 Extract task.
- **Files:** `Docs/draw-extract_Plan.md`
- **What changed:** Task scoped; implementation starting.
- **Verification:** N/A

## 2026-05-25 — Gfx extract module + Vk_Core hook

- **Plan ref:** Steps 1–5 (types, extract impl, core wiring, remove DrawObjects, vcxproj).
- **Files:** `VulkanDesktop/Gfx/Gfx_DrawExtract.h`, `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp`, `VulkanDesktop/RenderCore/Vk_Core.h`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `VulkanDesktop/VulkanDesktop.vcxproj`, `VulkanDesktop/VulkanDesktop.vcxproj.filters`, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:** Added `Gfx_DrawInstance`, `Gfx_SceneEntity`, `Gfx_ExtractDrawInstances` (no Vulkan). Demo scene (house + monkey). Extract runs each frame in `DrawFrame`; removed dead `DrawObjects`. Archived S1 extract task in sprint plan.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run 4s no crash; log `[SCENE] Demo scene entities initialized: 2`, `[EXTRACT] entities=2 visible=2 draws=2`.

## 2026-05-25 — Log throttle fix

- **Plan ref:** Verification / hygiene.
- **Files:** `VulkanDesktop/RenderCore/Vk_Core.h`, `VulkanDesktop/RenderCore/Vk_Core.cpp`
- **What changed:** `[EXTRACT]` logs once per run (`myExtractLoggedOnce`); avoids spam when `myFrameNumber` is still 0 at `DrawFrame` entry.
- **Verification:** Rebuild exit 0; smoke-run shows single `[EXTRACT]` line per session.
