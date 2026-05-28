# Progress: gfx-vk-decoupling

**Plan:** [`gfx-vk-decoupling_Plan.md`](gfx-vk-decoupling_Plan.md)

---

## 2026-05-28 — Phase 2: plan authored

- **Plan ref:** full task plan creation from confirmed landing details (S2 placement, full migration scope, complete touch list)
- **Files:** `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`, `Docs/gfx-vk-decoupling_Plan.md`, `Docs/gfx-vk-decoupling_Progress.md`
- **What changed:** Added S2 task entry, synchronized architecture intent, and authored full implementation plan including complete file touch list.
- **Verification:** Build/smoke-run N/A - doc planning phase only.

---

## 2026-05-28 — P1: contract scaffolding (no behavior switch)

- **Plan ref:** `gfx-vk-decoupling_Plan.md` -> P1 contract definition
- **Files:** `Gfx/Gfx_RenderPacket.{h,cpp}`, `RenderCore/Vk_RenderBackend.{h,cpp}`, `RenderCore/Vk_FrameDrawPrep.{h,cpp}`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`
- **What changed:** Added backend-facing render packet contract and a RenderCore boundary validator. Wired packet construction/validation inside `Vk_FrameDrawPrep::Build` as non-invasive scaffolding while keeping existing execution path unchanged.
- **Verification:** MSBuild `Debug|x64` exit 0; 4s smoke-run passed; runtime log still shows expected `EXTRACT/BATCH/FillInstanceSlab/PERF` signals and no new `[ERROR]`.

---

## 2026-05-28 — P2: dual-path parity wiring (shadow consume checks)

- **Plan ref:** `gfx-vk-decoupling_Plan.md` -> P2 producer/consumer dual-path
- **Files:** `RenderCore/Vk_FrameDrawPrep.cpp`, `RenderCore/Vk_RenderBackend.{h,cpp}`, `RenderCore/Vk_Core.cpp`
- **What changed:** Moved packet construction to post-slab stage so packet carries final per-draw offsets, added packet-vs-legacy parity validator for draw/batch counts, and wired one-time runtime parity diagnostics in `DrawFrame` before future execution-path switch.
- **Verification:** MSBuild `Debug|x64` exit 0; 4s smoke-run passed; runtime log shows `Packet parity validated against legacy draw prep.` and no new render-path errors from this batch.

---

## 2026-05-28 — P3: packet consume path default + legacy fallback

- **Plan ref:** `gfx-vk-decoupling_Plan.md` -> P3 switch execution to packet consume path
- **Files:** `RenderCore/Vk_Core.h`, `RenderCore/Vk_ScenePasses.{h,cpp}`
- **What changed:** Added packet-based draw record entry points in `Vk_ScenePasses`, switched `RecordScene` to use packet consume path by default, and retained automatic fallback to legacy consume path when parity/check gate is not satisfied.
- **Verification:** MSBuild `Debug|x64` exit 0 after one compile-fix iteration; 4s smoke-run passed; runtime log shows `Scene record switched to packet consume path.` and no new render-path errors from this batch.

---

## 2026-05-28 — P4: coupling cleanup on Vk record interfaces

- **Plan ref:** `gfx-vk-decoupling_Plan.md` -> P4 coupling cleanup and guardrails
- **Files:** `RenderCore/Vk_ScenePasses.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`
- **What changed:** Removed `Gfx_ExtractResult`-based record interfaces from `Vk_ScenePasses`/`Vk_Core` execution surface and consolidated record entry points around packet consume APIs. Legacy fallback now re-materializes packet-shaped pass data instead of calling extract-result APIs directly.
- **Verification:** MSBuild `Debug|x64` exit 0; 4s smoke-run passed; runtime log confirms packet consume path remains active (`Scene record switched to packet consume path.`), with no new render-path errors from this batch.

---

## 2026-05-28 — P4.1: packet-only runtime prep in RenderCore

- **Plan ref:** `gfx-vk-decoupling_Plan.md` -> P4 coupling cleanup and guardrails (runtime surface narrowing)
- **Files:** `RenderCore/Vk_FrameDrawPrep.{h,cpp}`, `RenderCore/Vk_Core.cpp`, `RenderCore/Vk_ScenePasses.cpp`
- **What changed:** Removed `Vk_FrameDrawPrep` runtime dependence on `myExtract/my*BatchRuns`; draw metrics, slab writes, and scene record consumption now use `myFramePacket` as the sole runtime payload. `Vk_ScenePasses` no longer falls back to extract-result based record in RenderCore.
- **Dependency assessment:** Remaining `Gfx_ExtractResult` usage is now contained inside Gfx draw-stream production (`Gfx_FrameDrawStreamOutput`) and no longer required by RenderCore runtime APIs. No Gfx module move to Vk section is needed in this phase.
- **Verification:** MSBuild `Debug|x64` exit 0; 4s smoke-run passed; runtime log keeps packet consume signal (`Scene record switched to packet consume path.`). Code search confirms no `Gfx_ExtractResult` usage remains under `RenderCore/`.

---

## 2026-05-28 — Closeout prep: Vk_Core legacy cleanup + plan status update

- **Plan ref:** `gfx-vk-decoupling_Plan.md` closeout readiness update
- **Files:** `RenderCore/Vk_Core.{h,cpp}`, `RenderCore/Vk_ScenePasses.cpp`, `Docs/gfx-vk-decoupling_Plan.md`, `Docs/gfx-vk-decoupling_Progress.md`
- **What changed:** Removed obsolete packet-path runtime toggle from `Vk_Core`, tightened DrawFrame contract comment to packet-only wording, and simplified `Vk_ScenePasses` path gate to packet validity only. Updated plan status and phase checklist to `Ready for closeout validation`.
- **Verification:** MSBuild `Debug|x64` exit 0; 4s smoke-run passed on closeout-prep batch.
