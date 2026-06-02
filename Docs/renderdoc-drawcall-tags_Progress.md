## 2026-06-02 — [P1 / planning and landing]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `Docs/renderdoc-drawcall-tags_Plan.md`, `Docs/renderdoc-drawcall-tags_Progress.md`
- **What changed:** Confirmed user landing details (F12 trigger, pass+per-draw detailed tags, startup-gated RenderDoc initialization) and created execution plan for implementation.
- **Verification:** N/A — planning/docs only

## 2026-06-02 — [P2 / startup gate, F12, draw tags]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/Util/Util_EngineConfig.h`, `VulkanDesktop/Util/Util_EngineConfig.cpp`, `VulkanDesktop/App/Application.h`, `VulkanDesktop/App/Application.cpp`, `VulkanDesktop/RenderCore/Vk_Core.h`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `VulkanDesktop/RenderCore/Vk_ScenePasses.h`, `VulkanDesktop/RenderCore/Vk_ScenePasses.cpp`, `Docs/CLI.md`
- **What changed:** Added `--renderdoc` startup gate and exposed config query; wired app init to enable RenderDoc path in `Vk_Core`; added F12 edge-trigger request in main loop; added command-buffer pass labels plus per-draw detailed tags (`Pass/Draw/Mesh/Material/Entity`) through debug-utils label helpers.
- **Verification:** Pending build + smoke run.

## 2026-06-02 — [P3 / build and smoke verification]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `Docs/renderdoc-drawcall-tags_Progress.md`
- **What changed:** Verified compile and runtime behavior for both default smoke path and `--renderdoc` startup path.
- **Verification:** Build `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` exit `0`; smoke `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit `0` with logs `[SCENE] LoadSceneResources completed`, `[APP] Smoke dwell reached (6.000000s); requesting exit.`, `[SCENE] UnloadScene: GPU scene resources released.`; startup gate check `.\VulkanDesktop.exe --no-validation --smoke-frames 2 --renderdoc` exit `0` with `[CONFIG] renderdoc=enabled`, `[RENDER] VK_EXT_debug_utils command labels enabled.`, and `renderdoc.dll` missing warning on this machine (`Requested by --renderdoc, but renderdoc.dll is not loaded/found`).

## 2026-06-02 — [P4 / RenderDoc abstraction refactor]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/RenderCore/Vk_RenderDoc.h`, `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`, `VulkanDesktop/RenderCore/Vk_Core.h`, `VulkanDesktop/RenderCore/Vk_Core.cpp`, `VulkanDesktop/VulkanDesktop.vcxproj`, `VulkanDesktop/VulkanDesktop.vcxproj.filters`
- **What changed:** Moved RenderDoc runtime attach, capture trigger, pending capture processing, and debug-utils label plumbing out of `Vk_Core` into dedicated `Vk_RenderDoc` class; `Vk_Core` now only delegates through this abstraction.
- **Verification:** Build `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` exit `0`; smoke `.\VulkanDesktop.exe --no-validation --smoke-frames 2 --renderdoc` exit `0` with `[CONFIG] renderdoc=enabled`, `[RENDER] VK_EXT_debug_utils command labels enabled.`, and RenderDoc startup gate warning preserved when `renderdoc.dll` is unavailable.

## 2026-06-02 — [P5 / auto-download and self-bootstrap]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/RenderCore/Vk_RenderDoc.h`, `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`, `Docs/CLI.md`
- **What changed:** Added RenderDoc bootstrap path: when `--renderdoc` is enabled and local `renderdoc.dll` cannot be loaded, runtime downloads official stable ZIP (`renderdoc.org/stable/1.44/RenderDoc_1.44_64.zip`), extracts to `ThirdParty/RenderDocBootstrap`, locates `renderdoc.dll`, then loads/injects it.
- **Verification:** Cancelled by updated requirement.

## 2026-06-02 — [P6 / remove auto-download, load from lib]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/RenderCore/Vk_RenderDoc.h`, `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`, `Docs/CLI.md`
- **What changed:** Removed all runtime auto-download/extract logic. `--renderdoc` path now only loads from already available `renderdoc.dll`: process/default DLL search path first, then explicit repo `lib/renderdoc.dll`.
- **Verification:** Pending rebuild + smoke check after rollback of downloader path.

## 2026-06-02 — [P7 / RenderDoc API compatibility fix]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/RenderCore/Vk_RenderDoc.h`, `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`, `VulkanDesktop/RenderCore/Vk_Core.cpp`
- **What changed:** Fixed capture pipeline for older/newer RenderDoc binaries by adding `RENDERDOC_GetAPI` multi-version negotiation fallback (1.7→1.0), and switched F12 capture path to explicit `StartFrameCapture`/`EndFrameCapture` frame wrapping instead of export-only trigger dependency.
- **Verification:** Build `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` exit `0`; smoke `.\VulkanDesktop.exe --no-validation --smoke-frames 2 --renderdoc` exit `0` with logs `RenderDoc API version 1.6.0 (requested 10600)` and `RenderDoc API attached`.

## 2026-06-02 — [P8 / move RenderDoc init timing earlier]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/RenderCore/Vk_RenderDoc.h`, `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`, `VulkanDesktop/RenderCore/Vk_Core.cpp`
- **What changed:** Split RenderDoc init into two phases: `InitRuntime()` before Vulkan instance/device initialization, and `BindVulkanHandles(...)` after device creation for handle binding + debug label function load.
- **Verification:** Pending rebuild after timing change.

## 2026-06-02 — [P9 / passive injection mode]

- **Plan ref:** `Docs/renderdoc-drawcall-tags_Plan.md`
- **Files:** `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp`, `Docs/CLI.md`
- **What changed:** Switched RenderDoc discovery to passive mode only: `GetModuleHandle("renderdoc.dll")`; removed all active `LoadLibrary` and repo `lib` probing behavior.
- **Verification:** Pending rebuild.
