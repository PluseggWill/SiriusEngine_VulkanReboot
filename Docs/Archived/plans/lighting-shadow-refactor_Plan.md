# Plan: lighting-shadow-refactor — S5 debt cleanup + G4 parity

**Status:** Closed (2026-06-13)  
**Progress:** [`Archived/plans/lighting-shadow-refactor_Progress.md`](Archived/plans/lighting-shadow-refactor_Progress.md)  
**Parent:** G4 acceptance (`hybrid-deferred-epic_Plan.md`) · reopens S5 closed debt  
**Depends on:** S4 PBR + G-buffer ✓ · S5 scaffold ✓ (broken/incomplete)

---

## 1. Problem

S5 was marked closed (2026-06-12) but **HybridDeferred shadows are visually wrong** on Sponza, and Khronos-alignment work (`khronos-shadow-map`) left **duplicate CPU paths, dead UBO fields, and misleading shader modules**. IBL runs but **prefilter mips are missing** while shaders assume 5 LOD levels; direct vs IBL energy models diverge.

| Symptom | Likely cause | Severity |
|---------|--------------|----------|
| Missing / swimming shadows when camera moves | Shadow pass reuses **view-frustum culled** opaque draw list | **P0** |
| Shadow on wrong lights (future) | `DeferredLighting.frag` applies visibility to **every** cluster light | P1 |
| ForwardLit shadows stale | Shadow pass never recorded on ForwardLit preset | P1 |
| Rough surfaces too glossy indoors | `roughness * 4.0` LOD with **single-mip** prefilter asset | P0 (IBL) |
| Grazing-angle energy leak | IBL diffuse lacks `(1-F)` vs direct `kD` | P1 (IBL) |
| Confusing maintenance | `PbrIbl.glsl` mixes IBL + shadow; dead helpers; `PatchLightViewProj` duplicates upload | P2 |

**Reference implementations (target behavior, not copy-paste):**

| Topic | Reference | What we take |
|-------|-----------|--------------|
| Directional shadow matrix | [Khronos Vulkan-Samples `shadows/`](https://github.com/KhronosGroup/Vulkan-Samples/tree/main/shaders/hpp/13-shadows) | Reversed-Z ortho, `GREATER` compare, raster bias, world-pos receiver sample |
| Shadow caster set | LearnOpenGL shadow mapping; Filament shadow pass | **Separate caster draws** from view culling |
| Split-sum IBL | LearnOpenGL PBR + Epic split-sum notes | `(1-F)*kD` diffuse, `F*A+B` specular, prefilter mip chain |
| Module layout | glTF Sample Viewer / Filament shader split | `PbrDirect` / `PbrIbl` / `PbrShadow` — single eval path |

---

## 2. Goals (definition of done)

| ID | Deliverable | Objective test |
|----|-------------|----------------|
| R1 | **Correct HybridDeferred shadows** | Sponza floor/columns: stable contact shadow; debug view shows filled depth map; fly camera does not erase off-screen casters |
| R2 | **Unified lighting eval** | One `Pbr_EvalSceneAmbient` + one shadow helper; forward transparent = deferred math |
| R3 | **Slim `GpuLightingGlobals`** | Fields used in GLSL match C++; no dead bias/texelSize unless wired |
| R4 | **IBL prefilter mips** | Load ≥5 mips OR pass `maxMip` uniform; rough surfaces visibly softer |
| R5 | **Energy-consistent IBL** | Diffuse uses `(1-F)*(1-metallic)` like `Pbr_EvalDirect` |
| R6 | **ForwardLit parity** | Record shadow pass before forward opaque when shadows enabled |
| R7 | **CI + smoke** | `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0 |

---

## 3. Non-goals

- Cascaded shadow maps (CSM) — Wishlist backlog
- 3×3 PCF tap loop — optional polish after R1
- Scene-load-only ortho cache (recompute per frame is OK if stable)
- DDGI / SSAO / post — S6+
- New shader permutations (M7 freeze unchanged)
- HDRI re-bake pipeline overhaul — minimal mip faces in-repo only

---

## 4. Touch list

### Shaders

| File | Change |
|------|--------|
| `Shader/PbrShadow.glsl` | **New** — extract shadow from `PbrIbl.glsl` |
| `Shader/PbrIbl.glsl` | IBL only; fix diffuse energy; mip from uniform |
| `Shader/PbrDirect.glsl` | Remove dead `Pbr_LitWithSunAndAmbient` |
| `Shader/LightingBindings.glsl` | Use `Pbr_EvalSceneAmbient`; slim shadow API |
| `Shader/DeferredLighting.frag` | Ambient helper; shadow **sun only** |
| `Shader/TriangleFrag_Lit*.frag` | Same ambient helper |
| `Shader_Generated/*` | Recompile + commit SPIR-V |

### Gfx / CPU

| File | Change |
|------|--------|
| `Gfx/Gfx_LightingGlobals.h` | Repurpose `iblParams.z` = prefilter max mip; trim shadow dead fields |
| `Gfx/Gfx_LightingMath.h` | Optional view-frustum-expanded bounds helper |
| `Gfx/Gfx_Bounds.*` | Shadow caster bounds (unculled opaque) |

### RenderCore

| File | Change |
|------|--------|
| `Vk_ShadowMapPass.*` | Caster packet input; pass-level debug label |
| `Vk_FrameUniformUploader.*` | **Single** matrix upload after shadow pass; remove `PatchLightViewProj` |
| `Vk_GBufferPass.cpp` | Shadow from caster packet; drop dead locals |
| `Vk_ScenePasses.cpp` | ForwardLit: shadow pass before opaque |
| `Vk_FrameDrawStream` / packet build | Build `myShadowCasterPass` (unculled opaque) |
| `Vk_IblResources.*` | Load prefilter with mip chain (or manifest mip count) |
| `Vk_DescriptorSystem.cpp` | Forward sky binding: use or drop |

### Assets / scripts

| File | Change |
|------|--------|
| `Data/Environments/default/manifest.json` | Optional `prefilterMipCount` |
| `Scripts/Generate-DefaultIblAssets.ps1` | Emit prefilter mip faces if missing |

---

## 5. Ordered steps

### Step 1 — Shadow correctness (P0)

1. Add `Gfx_BuildShadowCasterPacket` — all active opaque entities, **no frustum cull**.
2. Wire shadow pass to caster packet (HybridDeferred + ForwardLit).
3. Remove duplicate matrix compute: upload `GpuLightingGlobals` **once** after `RecordDraw` (delete `PatchLightViewProj` + early `UpdateLightingGlobals` matrix write).
4. `DeferredLighting.frag`: apply `Pbr_ShadowVisibility` only when light is directional sun (cluster index 0 / flag).
5. Verify: Sponza manual + shadow debug view.

### Step 2 — Shader module split + dead code (P2)

1. Create `PbrShadow.glsl`; update includes in lit/deferred/bindings.
2. Remove unused `N,L` from shadow API; drop `Pbr_ShadowProject` bool return.
3. Delete `Pbr_LitWithSunAndAmbient`; consolidate ambient via `Pbr_EvalSceneAmbient`.
4. Slim `shadowParams` to `{enabled}` (+ optional texelSize if PCF added later).

### Step 3 — IBL quality (P0–P1)

1. Load prefilter cubemap with full mip chain (5 levels minimum).
2. Set `iblParams.z = maxMipLevel` in `Gfx_BuildLightingGlobals`.
3. `Pbr_SamplePrefilter`: `lod = roughness * maxMipLevel`.
4. `Pbr_EvalIbl`: `kD = (1.0 - F) * (1.0 - metallic)` on diffuse.
5. Optional: `Pbr_FresnelSchlickRoughness` for specular F.

### Step 4 — ForwardLit parity + docs (P1)

1. Record shadow pass in `RecordForwardLit` when shadows enabled.
2. Document sky = deferred-only (or add forward sky pass — defer if scope creep).
3. Update `SprintOutcomeValidation.md` S5 checklist notes if behavior changes.

### Step 5 — Closeout

1. `Verify-CI.ps1` + `Verify-Smoke.ps1`.
2. Progress closeout; archive Plan+Progress; Active-Plan note if G4 unblocked.

---

## 6. Verification

| Gate | Command | Pass |
|------|---------|------|
| G0 | `powershell -File Scripts/Verify-CI.ps1` | exit 0 |
| G0-smoke | `powershell -File Scripts/Verify-Smoke.ps1` | exit 0 |
| Manual | Sponza HybridDeferred, shadows+IBL on | floor shadow stable; interior specular softens with roughness |
| Debug | Render debug → Shadow map | grayscale depth in frustum, magenta OOB |

---

## 7. Risks

| Risk | Mitigation |
|------|------------|
| Caster pass cost (full scene every frame) | Accept for v0; later merge with expanded bounds cull |
| Prefilter asset size | Keep 128² mips in default env |
| UBO layout change | Update C++ + all GLSL + GfxTests if any |
| Front-face shadow cull (Z-up) | Keep documented; revisit with back-face + depth bias tuning |

---

## 8. Agent quick start

1. Read this plan + `EngineArchitecture.md` §7 pass topology.
2. Implement **Step 1** first — shadow wrongness is highest user pain.
3. Log checkpoints in `_Progress.md` per step.
4. Do not commit without user confirmation.
