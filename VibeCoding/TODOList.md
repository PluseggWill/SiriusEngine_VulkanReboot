# TODO List — Follow-up & Roadmap

This document tracks near-term fixes and longer-term work toward a **small but real game engine foundation**: a **runnable gameplay loop in a coherent scene**, plus a **controlled place to turn rendering features on/off** and observe **visual quality vs performance**.

---

## 0. North star (what “done” looks like)

- **Engine**: Deterministic startup, stable asset/shader pipeline, clear module boundaries (window/input, render, scene, gameplay), minimal editor-free workflow (config + data on disk).
- **Product slice**: At least **one playable vertical slice** — a scene the player can navigate or interact with, a simple objective or loop (even tiny), and **fail-soft** behavior (no silent black screen without logs).
- **Rendering lab**: Each major rendering choice is **feature-flagged** or selectable at runtime/config, with **paired captures**: timestamps/GPU queries/frame time + optional screenshots or buffer dumps for A/B.
- **Evidence**: Repeatable **benchmark scenes** and a short **runbook** so a new machine can build, run the slice, and reproduce numbers within a documented variance band.
- **Data architecture (non-negotiable)**: The **data side** of the engine (simulation inputs, scene/runtime state, and **render-facing extracts**) is **data-oriented**: hot paths favor **SoA / columnar storage**, **stable indices** (with generation/slot validation where needed), **sequential scans** over pointer-heavy graphs, and **explicit transform/property buffers** prepared for upload or GPU consumption — not deep OO hierarchies traversed per frame for core work.

---

## 1. Workstream — Toolchain, assets & stability (existing follow-ups)

These items unblock everything else: if shaders, paths, or validation are flaky, gameplay and rendering experiments are not trustworthy.

### P0 (must fix first)

- [ ] Vendor a stable `dxc` build with SPIR-V codegen enabled into the repository (or pinned artifact source).
- [ ] Make shader compiler selection deterministic: prefer repo-local tools over system `PATH`.
- [ ] Add CI step to run shader compilation and fail fast on tool mismatch.
- [ ] Unify shader contracts (descriptor sets/bindings and entry points) between DXC and GLSLC paths.
- [ ] Review descriptor type strategy (`UNIFORM_BUFFER` vs `UNIFORM_BUFFER_DYNAMIC`) and lock one approach.
- [ ] Replace heuristic path probing with a robust asset root configuration system.
- [ ] Add startup checks that verify required resources (shader/model/texture) exist before initialization.

### P1 (important, next iteration)

- [ ] Add shader tool version logging (`dxc --version`, `glslc --version`) to build logs.
- [ ] Remove duplicated/legacy shader variants and keep one clear source of truth.
- [ ] Add pipeline creation diagnostics (full `VkResult` + stage/layout summary) for faster triage.
- [ ] Properly install and configure Vulkan validation layers for development machines.
- [ ] Add explicit startup diagnostics for layer discovery paths and missing layer details.
- [ ] Add optional runtime flag to force validation on/off (without code changes).
- [ ] Fix `VkInit::Pipeline_DynamicStateCreateInfo()` to avoid returning pointers to temporary containers.
- [ ] Expand `README.md` with one-machine setup steps (toolchain, build, run, troubleshooting).
- [ ] Add a "new machine bootstrap" guide with exact required versions and verification commands.

### P2 (nice to have / ongoing improvement)

- [ ] Document required runtime working directory behavior (or eliminate dependency on it).
- [ ] Add per-day or per-run log rotation to prevent indefinite log growth.
- [ ] Split logs by domain (build/runtime/rendering) with configurable verbosity.
- [ ] Add explicit crash summary section (last error + context) at shutdown on failure.
- [ ] Review and resolve linker warning `LNK4098` (runtime library mismatch).
- [ ] Clean up warning-prone casts (e.g. `size_t` to `uint32_t`) with safe bounds checks.

---

## 2. Workstream — Engine foundation (runtime architecture)

Goal: grow `Vk_Core` and surrounding code into a **layered engine** without collapsing everything into one translation unit.

### Core runtime

- [ ] Define a minimal **application lifecycle** (init → load scene → tick/update → render → shutdown) separate from Vulkan bootstrap.
- [ ] Introduce a thin **scheduler** or fixed update step (vs render step) so gameplay and rendering rates are conceptually separated.
- [ ] Central **configuration** (file + CLI overrides): window size, vsync, feature toggles, asset root, log level.
- [ ] **Input abstraction** (keyboard/mouse first; gamepad later) consumed by gameplay and camera, not by `Vk_Core` directly.
- [x] **Debug fly camera** (WASD, Q/E, RMB look, delta-time): `Util_InputSnapshot` + `UtilInput::Sample`, `Vk_Camera::ApplyInput`, `BeginFrame` ordering, ImGui **Camera** panel — see `VibeCoding/fps-camera_Plan.md`. *Note: sampling still invoked from `Vk_Core` until a dedicated input/simulation module exists.*

### Scene & objects

- [ ] Stable **scene description**: list of entities with transform, mesh, material, visibility/layer flags; load from data (JSON/YAML/TOML — pick one and stay consistent).
- [ ] **Transform hierarchy** (parent/child) or flat world matrices with a clear upgrade path to hierarchy.
- [ ] **Resource handles** for meshes/textures/materials; avoid reloading assets every frame.
- [ ] **Lifetime rules** for GPU resources vs CPU scene edits (when to rebuild descriptors/pipelines).

### Data-oriented architecture (constraint + implementation)

*Rendering and gameplay should **read** compact buffers built from this layer; avoid “every `GameObject` virtual Update walks the world tree” as the primary execution model.*

- [ ] **SoA core state**: transforms (translation/rotation/scale or mat4), bounds, layer/mask, mesh/material **indices** in separate contiguous arrays; optional parallel arrays for gameplay tags.
- [ ] **Stable entity id**: index + generation (or slot map); destroy/recycle does not silently revive stale handles.
- [ ] **Extract step for rendering**: each frame (or phase), build **render lists** from SoA (e.g. visible object indices, sorted draw keys) — render thread / `Vk_Core` consumes **arrays + counts**, not random scene graph walks.
- [ ] **GPU resource indirection**: materials/meshes as **tables** (index → buffer/descriptor index); draw records store **indices**, resolve to Vulkan handles only at batch/submit boundary.
- [ ] **Uniform / instance data**: ring buffers or **large UBO/SSBO** slices written sequentially; avoid per-object small allocations on the hot path.
- [ ] **Job-friendly boundaries**: system updates declare read/write **column sets** so parallel for-each over ranges is possible later without rewriting data layout.

### Gameplay hooks (engine-side, not full game design)

- [ ] **Player controller** contract: movement, look, interaction ray or overlap query hook.
- [ ] **Simple game state** machine or mode stack (e.g. Play / Pause / Dev overlay).
- [ ] **Event channel** (queue or immediate) for gameplay ↔ UI ↔ debug commands.

---

## 3. Workstream — Playable vertical slice (one scene + one loop)

Goal: prove the engine is **for games**, not only tutorials — something a stranger can “play” for one minute.

### Scene & content

- [ ] Author **one primary benchmark/play scene** (layout, lighting intent, camera start pose) checked into `Data/`.
- [ ] Ensure **all referenced assets** (models, textures, sky if any) are present or procedurally substituted with clear warnings.
- [ ] Optional: tiny **second scene** used only for load-time and streaming smoke tests.

### Gameplay (minimal but real)

- [ ] **Objective**: e.g. reach a marker, collect N items, survive T seconds, or toggle all lights — pick one and finish it.
- [ ] **Win/lose or completion** feedback (on-screen text overlay or console + log summary).
- [ ] **Restart** flow from keyboard without restart of the process.

### Presentation

- [ ] Minimal **HUD** or on-screen debug: FPS, frame time, active rendering preset name.
- [ ] **Pause** and **frame advance** (dev) to inspect flicker or TAA behavior.

---

## 4. Workstream — Rendering feature lab (quality vs performance)

Goal: every listed feature should be **toggleable**, **measured**, and **documented** with one paragraph on expected cost and visual tradeoff.

### Infrastructure for experiments

- [ ] **Rendering preset** system: `Low / Base / High / Custom` mapping to concrete flags.
- [ ] **Hot reload** or restart-fast path for shaders/pipelines in dev builds only.
- [ ] **GPU timer queries** (where supported) + CPU frame timing; log **p50/p95** over N frames.
- [ ] **Screenshot capture** keyed to preset + camera pose for visual regression (manual compare first; automated diff later).

### Pipeline design aligned with data-oriented submission

- [ ] **Draw stream model**: represent work as **sorted batches** (e.g. sort key = pipeline hash, material id, mesh id, depth bucket) over a flat **draw command array** built from the render extract.
- [ ] **Minimal per-draw Vulkan chatter**: batch `vkCmdBindPipeline` / descriptor sets / vertex/index buffers; prefer **multi-draw** or **instancing** when many objects share mesh/material.
- [ ] **Bindless vs traditional (decision)**: if using bindless (descriptor indexing / `VK_EXT_descriptor_buffer`), document table layout; else keep **material batches** with shared descriptor sets + dynamic offsets / push constants for **material index**.
- [ ] **Instance/step rate data**: vertex/instance buffers or SSBO filled from SoA columns (e.g. object-to-world, material params) with **tight struct layout** matching shader `struct` alignment.
- [ ] **Phase graph (CPU)**: culling → LOD select → key sort → **sequential record** of command buffers — each phase reads/writes **known buffers**, no hidden aliasing across systems.
- [ ] Optional stretch: **GPU culling / compaction** (draw indirect) — keep CPU SoA as source of truth until GPU path is proven equivalent.

### Features to expose as experiments (prioritize order as needed)

- [ ] **MSAA** vs post-process AA (or none): sample count vs edge stability.
- [ ] **Shadow mapping** (single cascade first): resolution, PCF taps, acne vs cost.
- [ ] **IBL / environment** vs simple directional: HDR env, mip chain, BRDF LUT cost.
- [ ] **PBR parameter sweeps**: roughness/metalness floors; texture resolution ladders.
- [ ] **Tonemapping / exposure**: fixed vs eye adaptation; impact on bright speculars.
- [ ] **Bloom / glare** (optional): threshold, knee, mip chain cost.
- [ ] **Deferred vs forward** (long-term): only if the engine commits to G-buffer complexity; otherwise document why forward stays default for the slice.
- [ ] **GPU particles or decals** (optional): fill-rate vs overdraw story.
- [ ] **Occlusion / LOD** (optional): hierarchical Z or CPU sphere tests vs polycount.

### Safety & correctness

- [ ] Validation-friendly paths for each feature (no duplicate layout sets per toggle without rebuild rules).
- [ ] **Graceful degradation**: if a feature is unsupported on the GPU, fall back and log once.

---

## 5. Workstream — Measurement, profiling & regression gates

- [ ] **Standard benchmark procedure**: scene, camera path (fixed or scripted), duration, warmup frames, output CSV or JSON.
- [ ] Integrate or document **RenderDoc** capture expectations for each major feature toggle.
- [ ] **CI smoke**: headless or shortest run that only validates init + one frame (may require mock surface or offscreen path — track as stretch).
- [ ] Track **memory** (VMA budgets) for large textures and shadow maps.
- [ ] Keep a **changelog of rendering presets** when defaults move (so A/B numbers stay interpretable).

---

## 6. Workstream — Documentation & collaboration

- [ ] **Engine overview** diagram (modules + data flow) in `README.md` or `Docs/` once structure stabilizes.
- [ ] **“How to add a rendering experiment”** checklist: shader, pipeline flag, preset entry, benchmark note.
- [ ] **Troubleshooting matrix**: black screen, validation spam, wrong working directory, missing layers.
- [ ] **License and third-party** inventory (`lib/`, SDK binaries) for redistribution clarity.

---

## 7. Workstream — Code maintainability & hygiene

*From static code review (2026-05-20). Goal: keep `VulkanDesktop` **simple, readable, and safe to change**. Some items overlap §1 (toolchain) and §2 (architecture); close here when the fix is localized, pull upward when it unlocks a larger milestone.*

### Guiding rules (apply to new code and refactors)

- **Single responsibility**: one module, one reason to change (`Vk_Core` = Vulkan backend; not input, asset policy, or scene authoring).
- **Explicit boundaries**: upper layers output plain structs/arrays (`DrawInstance`, UBO slices); `Vk_Core` records commands from prepared data only.
- **Fail fast**: validate files, mesh attributes, and shader entry points **before** using sizes or pointers.
- **No silent globals**: prefer passing a render/resource context over `Vk_Core::GetInstance()` from `Gfx_*` / `Util_*`.
- **Config matches behavior**: if a flag or helper exists (dynamic viewport, MSAA, feature toggles), the runtime path must use it or the dead code must go.
- **Delete or finish**: stale `TODO` / half-implemented draw paths create false “main paths”; resolve within one sprint or remove and track here.
- **Consistent diagnostics**: prefer `UtilLogger` over ad-hoc `std::cout` in engine code; fix typos in identifiers when touching an area (`availableLayers`, etc.).

### P0 (correctness — fix before new features)

- [x] **`Gfx_Mesh::LoadMesh`**: guard `texcoord_index` (and empty `attrib.texcoords`); default UV when missing — avoids crash on OBJ without UVs (`Vk_Types.cpp`).
- [x] **`UtilLoader::LoadTexture`**: check `stbi_load` result **before** computing `imageSize` / mip count; log resolved path on failure (`Util_Loader.cpp`).
- [x] **Queue family selection**: treat graphics queue as transfer-capable when no dedicated transfer family exists; relax `Vk_QueueFamilyIndices::isComplete()` accordingly (`Vk_DataStruct.h`, `Vk_Core.cpp`).

### P1 (structure & consistency)

- [ ] **`Vk_Core` decomposition (incremental)**: peel input sampling, resource tables, and draw-list build out of `Vk_Core` per `VibeCoding/EngineArchitecture.md` §3.1 — aligns with §2 “application lifecycle” and “input abstraction”.
- [ ] **Dynamic pipeline state**: either wire `VkInit::Pipeline_DynamicStateCreateInfo()` through `Vk_PipelineBuilder::BuildPipeline` (viewport/line width) or remove unused dynamic-state setup — related to §1 P1 “Fix `VkInit::Pipeline_DynamicStateCreateInfo()`”.
- [ ] **`DrawObjects` / render path**: finish multi-object draw + UBO update, or delete stub and document `RecordCommandBuffer` as the single path until extract/batch lands (§4 draw stream).
- [ ] **Remove temporary init hacks**: resolve `CreateMaterial` “TODO: Remove this”, `InitScene` / `myRenderObjects` wiring, and env-buffer “temp” creation (`Vk_Core.cpp`).
- [ ] **Debug messenger**: implement validation debug utils or document intentional omission (§1 validation layers).
- [ ] **Image queue sharing**: when transfer and graphics families differ, set concurrent sharing on images/buffers that cross queues (`Vk_Core::CreateImage` TODO).

### P2 (hygiene & developer experience)

- [ ] **Reduce `GetInstance()` coupling**: `Util_Loader`, `Gfx_Mesh::BuildBuffers` should receive allocator/device helpers via parameters or a slim `Vk_ResourceContext` instead of singleton access.
- [ ] **Unify debug output**: replace `CheckExtensionSupport` / `CheckValidationLayerSupport` `std::cout` dumps with `UtilLogger` (or gate behind a dev flag).
- [ ] **Global compile-time toggles**: move `ENABLE_ROTATE`, `USE_RUNTIME_MIPMAP`, shader paths in `Vk_Core.cpp` toward central config (§2 configuration) with defaults documented.
- [ ] **Stale TODO sweep**: triage remaining `Vk_Core` TODOs (uniform buffer array, merged setters, device suitability checks) — implement, defer to §2/§4, or delete comment.
- [x] **Comment conventions**: `.cursor/rules/cpp-comments.mdc` + core contract comments (descriptors, UBO packing, queues, mesh UV guards).

---

## 8. Parking lot (ideas — not committed until pulled upward)

- Editor or in-engine property panel (likely out of scope until slice ships).
- Networking or physics (only after single-player slice is stable).
- Cross-platform windowing beyond Windows (defer until product asks for it).

---

*Maintenance: when closing items, prefer a one-line note in git commit or a dated entry in a progress log so preset/benchmark history stays traceable. Code-hygiene items live in **§7**; update `VibeCoding/EngineArchitecture.md` when module boundaries change.*
