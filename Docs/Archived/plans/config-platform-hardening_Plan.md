# Plan: config-platform-hardening

**Status:** Closed (2026-06-02)  
**Progress:** [`config-platform-hardening_Progress.md`](config-platform-hardening_Progress.md)  
**Parent:** [`Archived-Plan.md`](../../Archived-Plan.md) **P1** (config-platform-hardening)  
**Covers hardening:** #7, #30, #31  
**Prerequisite:** P0 closed ([`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md)); peel track closed ([`Archived/plans/vk-core-world-peel_Plan.md`](Archived/plans/vk-core-world-peel_Plan.md)) — App already owns `WorldState` / debug UI.  
**Related:** [`Archived/plans/central-config_Plan.md`](Archived/plans/central-config_Plan.md) (S2 singleton API — **this plan replaces file-static globals with an instance**), [`shader-bindless-policy_Plan.md`](shader-bindless-policy_Plan.md) (touch `Vk_Bindless` env read in Phase 2).

**Process:** Each phase ends with **hard acceptance** below; run [`Scripts/Verify-CI.ps1`](../Scripts/Verify-CI.ps1) + [`Scripts/Verify-Smoke.ps1`](../Scripts/Verify-Smoke.ps1) unless the phase is docs-only (Phase 2 doc slice).

---

## Problem

| Signal | Today (baseline — log in Phase 0) |
|--------|-------------------------------------|
| Config storage | ~20 file-static `g*` variables in `Util_EngineConfig.cpp` anonymous namespace |
| API | `UtilEngineConfig::Initialize` + `Get*` free functions; **~15 call sites** outside `Application` (Gfx, Util panels, RenderCore, `GfxTests`) |
| Tests | `GfxTests` calls `Initialize` once per process — **cannot** compare two configs without refactor |
| Platform | `Windows.h` / `_dupenv_s` in `Vk_Bindless.cpp`, `Vk_RenderDoc.cpp`, `GfxTests`; cross-platform only mentioned in Wishlist |
| Frame errors | `Vk_SwapchainHost::AcquireNextImage` returns `false` on OUT_OF_DATE (OK); **submit/present** and non-OUT_OF_DATE acquire **throw** → `Application::Run` exits |

**Symptoms:** brittle CI/local runs when resize or recoverable VK errors occur; hidden global state blocks headless/multi-config tests; platform assumptions scattered.

---

## Goal

1. **`Util_EngineConfig` instance** owned by `Application`, passed as `const Util_EngineConfig&` (or thin `AppConfigView`) — no mutable file-level config globals.
2. **Platform honesty** — Windows + MSVC + MSBuild locked in architecture; every Win32/env escape hatch **listed and justified** (or removed).
3. **Recoverable Vulkan frame errors** — OUT_OF_DATE / SUBOPTIMAL / transient submit-present failures **do not throw**; device lost requests graceful shutdown.

---

## Non-goals

- Linux/macOS port or GLFW abstraction of RenderDoc DLL load
- `VulkanDesktop.exe --run-tests` (P0 uses **`GfxTests.exe`** — stays)
- New `engine.json` keys or changing precedence rules (CLI > JSON > defaults — **locked** from central-config)
- `SIRIUS_STRICT_ASSET_ROOT` env fail-fast (optional future in ci-verification)
- Refactoring init-time `throw` in `Util_EngineConfig` parse (invalid JSON still fails fast at startup)
- Peeling `UtilLightingPanel::Build` out of `DrawFrameGpu` (vk-core debt)

---

## Implementation order

| Order | Phase | Rationale |
|-------|-------|-----------|
| 0 | Baseline | Measurable before/after |
| 1 | Config instance | Unblocks testable precedence; reduces hidden coupling before more App wiring |
| 2 | Platform honesty | Mostly docs + inventory; small code comments when touching bindless |
| 3 | Recoverable VK errors | Depends on stable frame loop in App + `DrawFrameGpu` |

**Suggested PR slices (Phase 1):** 1A struct + `Application::myConfig` + InitApp; 1B Util/Gfx; 1C RenderCore; 1D delete globals + GfxTests precedence test.

---

## Phase 0 — Baseline

**Work:** Record counts; no behavior change.

| Signal | How to measure |
|--------|----------------|
| File-static config | `rg "^[a-zA-Z].* g[A-Z]" VulkanDesktop/Util/Util_EngineConfig.cpp` |
| `UtilEngineConfig::Get` call sites | `rg "UtilEngineConfig::" VulkanDesktop --glob "*.{cpp,h}"` |
| `friend` on `Vk_Core` | `rg "friend class" VulkanDesktop/RenderCore/Vk_Core.h` (expect 0 post-peel) |
| Swapchain throws | `rg "throw std::runtime_error" VulkanDesktop/RenderCore/Vk_SwapchainHost.cpp` |
| Win32 includes | `rg "#include <Windows.h>" VulkanDesktop` |

Log results in `{name}_Progress.md` when task is kicked off.

### Acceptance (hard)

| ID | Criterion | Proof |
|----|-----------|-------|
| P0-G0 | Build + GfxTests green | `powershell -File Scripts/Verify-CI.ps1` → exit **0** |
| P0-G0s | Smoke unchanged | `powershell -File Scripts/Verify-Smoke.ps1` → exit **0**; log contains `[SCENE] LoadSceneResources completed` and `[APP] Engine exited run loop normally` |
| P0-B1 | Baseline table committed | Progress log lists static `g*` count, external `Get*` site count, swapchain `throw` line count, Win32 file list |

---

## Phase 1 — `Util_EngineConfig` instance (#7)

### Design (locked)

| Item | Decision |
|------|----------|
| Type | `struct Util_EngineConfig` (rename namespace → type, or `Util_EngineConfig` struct inside namespace — **pick one in 1A, stay consistent**) |
| Load API | `bool LoadFromArgv( int argc, char** argv, std::string& outError )` or `static Util_EngineConfig LoadOrThrow(...)` — **must not** write file-static mutable state |
| Owner | `Application::myConfig` (member); `InitApp` loads; `Run` passes `const Util_EngineConfig&` into helpers |
| Early CLI | `VulkanDesktop.cpp` keeps `TryEarlyExitFromCli` — may stay static if it only parses `--help` without touching loaded config |
| Delegates | `Util_AssetConfig` / `Util_ValidationConfig` become **thin** pass-throughs taking `const Util_EngineConfig&`, or are deleted with call sites updated |
| Precedence | **Unchanged:** CLI > config file > code defaults (same keys as today) |

### Migration steps

1. **1A** — Introduce struct + load from argv; `Application` member; `Initialize`/`Get*` shim to single global **only** inside `Util_EngineConfig.cpp` (temporary).
2. **1B** — Thread `const Util_EngineConfig&` through **Util/** and **Gfx/** (`Util_Loader`, panels, `Gfx_DemoSceneSim`, `Gfx_ShaderPermutation`, `Util_PerfLog`).
3. **1C** — Thread through **RenderCore/** (`Vk_DescriptorSystem`, `Vk_ShaderEffectMeta`, `Vk_DevicePipelineCache`).
4. **1D** — Remove shims + all `g*` config statics; update `GfxTests` to use stack instances.

### Acceptance (hard)

| ID | Criterion | Proof |
|----|-----------|-------|
| P1-G0 | No regression | `Verify-CI.ps1` and `Verify-Smoke.ps1` → exit **0** |
| P1-A1 | No file-static config state | `rg "^\w+.*\sg(AssetRoot|Initialized|WindowWidth|Features)" VulkanDesktop/Util/Util_EngineConfig.cpp` → **0** matches |
| P1-A2 | No global getters outside config module | `rg "UtilEngineConfig::(Get|Is|Resolve|Log)" VulkanDesktop --glob "*.{cpp,h}"` → matches only in `Util_EngineConfig.{cpp,h}`, `VulkanDesktop.cpp` (`TryEarlyExitFromCli`), and **no** matches in `Util_AssetConfig.cpp` / `Util_ValidationConfig.cpp` (delegates removed or take `const Util_EngineConfig&`) |
| P1-A3 | Application owns instance | `Application.h` declares config member; `rg "UtilEngineConfig::Initialize" VulkanDesktop` → **only** `Util_EngineConfig.cpp` (if shim) **or** `Application.cpp` (if load is member method) — not in `GfxTests` without local instance |
| P1-A4 | **Dual-instance precedence test** | `GfxTests.exe` (or new test in `GfxTests_Main.cpp`): construct `Util_EngineConfig` **A** from temp JSON (`vsync: true`), **B** from argv `--vsync false` with same JSON; assert `A.GetVsync()!=B.GetVsync()` and CLI wins; asset-root: JSON path vs `--asset-root` override — **both** predicates pass |
| P1-A5 | Smoke log contract | Smoke log still has `[CONFIG] assetRoot=` with path under repo (existing `Assert-SmokeLog.ps1` / bootstrap contract) |
| P1-A6 | Init order preserved | `Application::InitApp`: config load → `UtilLogger::Init` → `Gfx_ShaderPermutation::Initialize` — order unchanged (grep InitApp sequence or manual log check) |

**Phase 1 done when:** P1-G0 and P1-A1 through P1-A6 all pass.

---

## Phase 2 — Platform honesty (#30)

### Work

1. **`EngineArchitecture.md`** — In §10 non-goals, add explicit bullet: **supported platform = Windows 10+ x64, MSVC, MSBuild**; link to platform inventory doc below.
2. **New `Docs/Platform.md`** (or § in bootstrap) — table: file path | Win32/API used | why Windows-only | future port owner (Wishlist).
3. **Inventory minimum** (must appear in doc):

   | Path | API |
   |------|-----|
   | `RenderCore/Vk_Bindless.cpp` | `_dupenv_s( "FORCE_MATERIAL_BATCH" )` |
   | `RenderCore/Vk_RenderDoc.cpp` | `LoadLibrary` / `GetProcAddress` |
   | `GfxTests/GfxTests_Main.cpp` | `GetModuleFileNameW` for repo root |
   | `App/Application.cpp` | GLFW (windowing; already cross-platform lib, **desktop target still Windows-only**) |

4. **`Vk_Bindless.cpp`** — Add file-header comment: dev-only env override; Windows CRT; S7 lab — do **not** silently port to `getenv` without Wishlist task.
5. **`Active-Plan.md` / `Wishlist.md`** — Remove duplicate “maybe cross-platform later” bullets from Active-Plan if any; single pointer: Wishlist owns port.

### Acceptance (hard)

| ID | Criterion | Proof |
|----|-----------|-------|
| P2-D1 | Architecture locked | `EngineArchitecture.md` §10 states Windows + MSVC + MSBuild only with link to `Docs/Platform.md` |
| P2-D2 | Inventory complete | `Docs/Platform.md` lists **every** path from Phase 0 Win32 `rg` list; no unlisted `#include <Windows.h>` under `VulkanDesktop/` |
| P2-D3 | Bindless documented | `Vk_Bindless.cpp` has comment block referencing `Platform.md` + `FORCE_MATERIAL_BATCH` |
| P2-G0 | Build green | `Verify-CI.ps1` → exit **0** (docs-only phases still run CI to catch accidental edits) |

**Phase 2 done when:** P2-D1, P2-D2, P2-D3, P2-G0 pass. Smoke not required if zero runtime behavior change (optional P0-G0s).

---

## Phase 3 — Recoverable Vulkan errors (#31)

### Design (locked)

| `VkResult` / condition | Behavior |
|------------------------|----------|
| `VK_ERROR_OUT_OF_DATE_KHR` / `VK_SUBOPTIMAL_KHR` on acquire or present | Existing `Recreate` path; **return** status — no throw |
| `myFramebufferResized` | Same as today — recreate, no throw |
| `vkQueueSubmit` != `VK_SUCCESS` (non-device-lost) | Log `[VK]` + `UtilLogger::Error`; **skip frame**; main loop continues |
| `vkQueuePresentKHR` failed, not OUT_OF_DATE/SUBOPTIMAL | Log `[VK]`; **skip frame**; no throw |
| `vkAcquireNextImageKHR` failed, not OUT_OF_DATE/SUBOPTIMAL | Log `[VK]`; **skip frame**; `PrepareFrameCpu` returns `false` (already partial) |
| `VK_ERROR_DEVICE_LOST` (acquire/submit/present) | Log fatal `[VK]`; set `Application` shutdown request — exit main loop cleanly, then `UnloadScene` / `Shutdown` |

**API shape:**

- Add `enum class Vk_FrameResult { Ok, SkipFrame, RequestShutdown };` (header in `RenderCore/`, e.g. `Vk_FrameResult.h`).
- `Vk_SwapchainHost::SubmitAndPresent` → returns `Vk_FrameResult` (or decomposed acquire/submit/present helpers).
- `Vk_Core::DrawFrameGpu` propagates `SkipFrame` / `RequestShutdown`; `Application::RunMainLoop` checks shutdown flag.

**Keep throwing** for: swapchain **creation** failures during init/recreate, command buffer begin/record failures (programmer/GPU resource errors), `WorldState` not bound, etc.

### Acceptance (hard)

| ID | Criterion | Proof |
|----|-----------|-------|
| P3-G0 | CI green | `Verify-CI.ps1` → exit **0** |
| P3-G0s | Smoke green | `Verify-Smoke.ps1` → exit **0** |
| P3-V1 | No throw on submit/present path | `rg "throw std::runtime_error" VulkanDesktop/RenderCore/Vk_SwapchainHost.cpp` → **0** matches inside `SubmitAndPresent` and inside `AcquireNextImage` failure branch (throws allowed in `Create*` helpers only) |
| P3-V2 | Typed result wired | `rg "Vk_FrameResult" VulkanDesktop/RenderCore` → used by `SubmitAndPresent` and `DrawFrameGpu` (or `PrepareFrameCpu` for acquire) |
| P3-V3 | Resize soak | **Manual:** run `VulkanDesktop.exe` from `x64\Debug` with repo asset root; drag resize edges **≥ 10 s**; no unhandled exception; log may contain `SWAPCHAIN` recreate warnings; process exits normally on close |
| P3-V4 | Resize soak (automation-friendly) | **Optional script** `Scripts/Verify-ResizeSmoke.ps1` documented in Progress — if added, must pass in Phase 3 closeout; if not added, P3-V3 manual steps are **mandatory** in Progress closeout |
| P3-V5 | Device lost path | Code review + **unit-style dev test:** documented steps in Progress to simulate or grep `VK_ERROR_DEVICE_LOST` → `RequestShutdown` → loop exits with `[APP] Engine exited run loop normally` without stack unwind from present |

**Phase 3 done when:** P3-G0, P3-G0s, P3-V1, P3-V2, P3-V3 (or P3-V4), P3-V5 pass.

---

## Global gates (task close)

| Gate | Command / criterion |
|------|---------------------|
| G0 | `Scripts/Verify-CI.ps1` |
| G0-smoke | `Scripts/Verify-Smoke.ps1` |
| Config | Phase 1 acceptance P1-A1–A6 |
| Platform | Phase 2 acceptance P2-D1–D3 |
| VK recover | Phase 3 acceptance P3-V1–V5 |
| Active-Plan | Hardening **#7, #30, #31** marked P1 ✓; line moved to `Archived-Plan.md` |
| Architecture | Update only if policy narrative changes (instance owner, frame error contract) |

---

## Touch list

| Area | Paths |
|------|--------|
| Config | `Util/Util_EngineConfig.{h,cpp}`, `Util_AssetConfig.*`, `Util_ValidationConfig.*` |
| App | `App/Application.{h,cpp}`, `VulkanDesktop.cpp` |
| Consumers | `Util_Loader.cpp`, `Util_PerfLog.{h,cpp}`, `Util_ScenePanel.cpp`, `Util_RenderDebugPanel.cpp`, `Gfx_DemoSceneSim.cpp`, `Gfx_ShaderPermutation.cpp` |
| RenderCore | `Vk_SwapchainHost.{h,cpp}`, `Vk_Core.{h,cpp}`, `Vk_DescriptorSystem.cpp`, `Vk_ShaderEffectMeta.cpp`, `Vk_DevicePipelineCache.cpp`, `Vk_Bindless.cpp` |
| Tests | `GfxTests/GfxTests_Main.cpp`, `VulkanDesktop.vcxproj` (if new test TU) |
| Docs | `Docs/Platform.md` (new), `EngineArchitecture.md`, `Active-Plan.md`, `bootstrap.md` (optional Platform link) |

---

## Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Large single PR | Minimum **4 PRs:** Phase 1A–1D, then 2, then 3 |
| Missed `Get*` call site | P1-A2 `rg` gate; CI fails on new global accessors |
| Config test flakiness | Use temp dir + unique JSON files in GfxTests; no repo `engine.json` mutation |
| Resize test flaky in CI | P3-V3 manual for closeout; optional script later |
| False “recoverable” swallow | Log every `[VK]` skip at Error level; count skips in Progress |

---

## Suggested kickoff checklist

- [x] User confirms kickoff → `config-platform-hardening_Progress.md`, README **Active now**
- [x] Phase 0 baseline logged
- [x] Phase 1 — config instance (1A→1D)
- [x] Phase 2 — Platform.md + architecture link
- [x] Phase 3 — `Vk_FrameResult` + non-throwing frame path
- [x] Closeout → archive Plan+Progress; update `Archived-Plan.md` / hardening index
