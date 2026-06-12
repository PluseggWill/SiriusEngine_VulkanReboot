# Sprint Outcome Validation

Validation runbook for sprint close-out. Use with [`Active-Plan.md`](Active-Plan.md) (queue + gates) and [`Archived-Plan.md`](Archived-Plan.md) (completed lines).

**Sprint map (2026-06 pivot):** lighting track **S4–S8** · simulation **S9** · geometry (deferred) **S10–S12** · infra **S13**. Old meshlet/mesh-shader § labels now map to **S10–S12** — see [index](#sprint-index).

## How to use

1. Finish all planned sprint tasks or explicitly defer remaining work.
2. Run the sprint-specific validation checklist below.
3. Record outputs (logs/screenshots/captures) in PR or progress notes.
4. Only then move completed sprint lines from **`Wishlist.md`** / queue → **`Archived-Plan.md`**.

---

## Sprint index

| Sprint | Theme | Milestone tag | Gate |
|--------|--------|---------------|------|
| S0–S3 | Bootstrap → FG v0 + GPU indirect | M2 | G0, G1 ✓ |
| P4 | Vertical slice v0 | — | G2 ✓ |
| **S4** | PBR + G-buffer | Lighting-1 | — |
| **S5** | IBL + skybox + shadows | Lighting-2 | — |
| **S6** | SSAO + Hi-Z | Lighting-3 | — |
| **S7** | Post + frame graph v1 | Lighting-4 | contributes to **G4** |
| **S8** | DDGI / GI | Lighting-5 | **G4** |
| **S9** | Simulation | — | G2 ✓ |
| **S10** | Meshlets | M3 | **G3** |
| **S11** | Mesh shader | M4 | S10 |
| **S12** | GPU mesh tasks | M5 | S11 |
| **S13** | Render lab infra | M6 | — |

**Default dev scene:** `Data/Scenes/sponza.json` (fetch required). **CI G0-smoke:** `stress.json` + `engine.stress.json` (unchanged).

---

<a id="validation-p0"></a>
## P0 validation (verify & measure)

**Plan:** [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md). **G0 (blocks M2):** Phase 0 + 1 only. **P0 done:** G0 + D1 + this section.

### CI / local G0

- `pwsh -File Scripts/Verify-CI.ps1` from repo root → exit **0** (MSBuild Debug\|x64, shader drift, `GfxTests.exe`).
- PR workflow green on paths in ci-verification Plan (or Docs-only PR skipped).

### G0-smoke (GPU; soft on default GitHub runner v1)

```powershell
$Repo = "<repo-root>"
& "$Repo\x64\Debug\VulkanDesktop.exe" `
  --asset-root $Repo `
  --config "$Repo\Config\engine.benchmark.json" `
  --scene Data/Scenes/smoke.json `
  --no-validation --smoke-frames 120 --smoke-seconds 6
pwsh -File "$Repo\Scripts\Assert-SmokeLog.ps1" -RepoRoot $Repo
```

**Stress / GPU-cull dogfood:** `pwsh -File Scripts/Verify-Smoke.ps1` (uses `stress.json`, not default `sponza.json`).

### Benchmark / perf (P0 done)

- `Config/engine.benchmark.json`: `vsync: false`.
- Optional: `--perf-log` + `Scripts/Perf-JsonlSummary.ps1` on 300 frames (p50 logged; no threshold v1).

### Adversarial archived claim (#26)

Before moving P0 lines to Archived-Plan:

1. Pick **one** line from [`Archived-Plan.md`](Archived-Plan.md) (or a completed `Archived/plans/*` claim).
2. Verify in **code or log** (not checkbox): e.g. “batch path uses 9 batch runs” → grep test fixture + smoke `[PERF]` or GfxTests expectation.
3. Record in PR / `_Progress.md`: claim text, file:line or log snippet, pass/fail.

### Peel / hygiene metrics (#27)

When closing **P1 peel** tasks (not required for P0 CI green): capture **one row** of objective numbers, e.g. `DrawFrame` LOC, `Vk_Core.cpp` LOC, `friend class` count in `RenderCore/`. Do not use task checkbox count as velocity metric.

| Step | Done when |
|------|-----------|
| Adversarial row filled | Claim + evidence in PR notes |
| Peel metrics (P1) | LOC/friend snapshot when peel lands |

---

<a id="validation-s0"></a>
## S0 validation

- Build Debug x64 succeeds from clean state.
- Shader compile path is deterministic (`Shader_Generated/*.spv` produced, no corruption).
- Startup checks and validation-layer toggles work as documented.
- Fresh-machine bootstrap steps in `Docs/bootstrap.md` are reproducible.

<a id="validation-s1"></a>
## S1 validation (M1)

- `demo.json` scene renders with expected entity/draw counts.
- Extract -> cull -> sort -> batch path logs expected first-frame diagnostics.
- Transparent-over-opaque correctness is visually verified.
- Instance slab and descriptor set policies (Set 0/1/2) behave as documented.

<a id="validation-stage1-forward"></a>
## Stage 1 forward gate (lighting epic)

- [`forward-stage1.md`](forward-stage1.md) §1 runbook + golden under `Docs/Assets/golden/forward-stage1/`; `[PERF]` row in §1.
- [`forward-stage1.md`](forward-stage1.md) §2 handoff reviewed and §3 gaps accepted before Stage 2 body.
- Smoke with `Config/engine.benchmark.json`: exit 0, `renderPreset=ForwardLit`.

<a id="validation-s2"></a>
## S2 validation

- Application lifecycle path runs cleanly: init -> update/render loop -> unload -> shutdown.
- `Util_EngineConfig` and CLI overrides behave as expected.
- Scene-load phase gates (A/B/C/D as applicable) match current sprint state.
- `Vk_Core` peel milestones compile/smoke pass without behavior regression.

### S2 closeout evidence (2026-06-02)

- Build: `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` -> exit 0.
- Smoke (historical): `x64\Debug\VulkanDesktop.exe --no-validation --smoke-seconds 6` -> exit 0 (implicit cwd discovery).
- **Template for new evidence:** use [P0 smoke command](#validation-p0) (`--asset-root` + `Assert-SmokeLog.ps1`).
- Log signals checked: `[SCENE] LoadSceneResources completed`, `[APP] Smoke dwell reached`, `[SCENE] UnloadScene: GPU scene resources released`, `[APP] Engine exited run loop normally`.

<a id="validation-s3"></a>
## S3 validation (M2 + FG v0)

- GPU frustum cull -> indexed indirect path is active.
- Draw count comes from GPU outputs (not per-object CPU draw loop).
- Fixed-camera parity check against CPU cull is within agreed tolerance.
- HybridDeferred opaque chain: `GBufferOpaque -> ClusterBuild -> DeferredLighting -> ForwardTransparent` logs once; `ForwardLit` fallback unchanged.

### S3 closeout evidence (2026-06-11)

**Plan:** [`Archived/plans/s3-m2-acceptance_Plan.md`](Archived/plans/s3-m2-acceptance_Plan.md).

**G0 (G1 parity — no GPU readback):**

```powershell
pwsh -File Scripts/Verify-CI.ps1
```

GfxTests cases: `cpu/gpu cull parity: demo overview`, `demo close viking view`, `layer mask filter` (`GfxTests_Main.cpp`).

**G0-smoke (CPU default + gpu-cull dogfood):**

```powershell
pwsh -File Scripts/Verify-Smoke.ps1
```

Pass 1 — CPU indirect (`engine.stress.json` + `stress.json`). Pass 2 — same scene with `--gpu-cull`.

**GpuCull log tokens** (pass 2; `Assert-SmokeLog.ps1 -Profile GpuCull`):

- `[CONFIG] gpuCull=true`
- `[CULL] … (gpu-deferred)`
- `GPU cull dispatch`
- `Scene record using GPU-filled slot indirect`

**Record path audit:** `--gpu-cull` without `--legacy-direct-draw` uses `vkCmdDrawIndexedIndirect` per draw from `myGpuCullIndirectBuffer` (`Vk_ScenePasses.cpp`); no `vkCmdDrawIndexed` on that path.

<a id="validation-p4"></a>
## P4 validation (vertical slice v0)

**Plan:** [`Archived/plans/p4-vertical-slice-v0_Plan.md`](Archived/plans/p4-vertical-slice-v0_Plan.md).

**CI:**

```powershell
pwsh -File Scripts/Verify-CI.ps1
```

GfxTests: `TestSliceSceneAssets` (slice JSON + on-disk asset paths).

**Manual play:**

```powershell
$Repo = "<repo-root>"
& "$Repo\x64\Debug\VulkanDesktop.exe" `
  --asset-root $Repo `
  --config "$Repo\Config\engine.slice.json"
```

- Fly to campfire (~`(3, 0, -4)`) → log `[SLICE] Objective won: reached target` + HUD **Complete!**
- Wait 120s without reaching target → `[SLICE] Objective lost: time limit`
- Press **R** or Scene panel **Restart current** → `[APP] Scene reload completed` (process stays open)

<a id="validation-s4"></a>
## S4 validation (Lighting-1 — PBR + G-buffer)

**Epic:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §B–C (partial).

**Prereq:** `powershell -File Scripts/Fetch-SponzaMcGuire.ps1` on benchmark machine.

**CI:**

```powershell
pwsh -File Scripts/Verify-CI.ps1
```

**Manual (Sponza):**

```powershell
$Repo = "<repo-root>"
& "$Repo\x64\Debug\VulkanDesktop.exe" `
  --asset-root $Repo `
  --config "$Repo\Config\engine.json" `
  --no-validation --smoke-seconds 6
```

- G-buffer encodes metallic/roughness (capture or debug viz).
- Deferred opaque shows specular response (not flat albedo-only) on stone/fabric.
- `ForwardLit` vs `HybridDeferred` on Sponza: parity notes + screenshots in PR.
- Log: `[SCENE] Parsed scene v1 name='sponza'`, `[FG] HybridDeferred: …`, exit 0.

<a id="validation-s5"></a>
## S5 validation (Lighting-2 — IBL + shadows)

**Deps:** S4 accepted.

- Skybox / environment visible at openings; IBL fills interior without blown exposure.
- Directional shadow map: ground contact shadows on Sponza; toggle in ImGui/config.
- Transparent forward uses same sun/IBL inputs as deferred.
- RenderDoc: shadow pass + deferred sample visible in capture.

<a id="validation-s6"></a>
## S6 validation (Lighting-3 — SSAO + Hi-Z)

**Deps:** S4; S5 recommended.

- Depth pyramid (Hi-Z) texture valid (debug mip overlay).
- SSAO darkens creases/contact regions; toggle on/off.
- No full-screen NaN/black regression on resize.

<a id="validation-s7"></a>
## S7 validation (Lighting-4 — post + frame graph v1)

**Deps:** S4–S6. Contributes to gate **G4**.

- `FrameGraphBuilder` (or documented v1) drives hybrid + shadow + AO + post chain.
- Tonemap/exposure applied; optional bloom documented if enabled.
- Preset `Low` / `Base` / `High` switch validation-clean.
- Benchmark runbook on Sponza: warmup, `--perf-log`, `Scripts/Perf-JsonlSummary.ps1` p50/p95 row in PR.
- `ForwardLit` ↔ `HybridDeferred` switch without validation errors.

<a id="validation-g4"></a>
## G4 validation (Stage 2 acceptance)

**Unlocks:** S8 DDGI. **Checklist:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) Acceptance + S7 evidence above.

- Opaque: `GBufferOpaque + DeferredLighting` with **full PBR** on Sponza.
- Transparent: forward pass composites over deferred depth.
- Shadow + IBL + AO/post verified on benchmark scene.
- Parity runbook: forward vs hybrid documented with captures.

<a id="validation-s8"></a>
## S8 validation (Lighting-5 — DDGI)

**Deps:** **G4** met. **Epic:** [`ddgi-lighting-epic_Plan.md`](ddgi-lighting-epic_Plan.md).

- DDGI preset off: identical to S7 baseline (visual + log).
- DDGI preset on: interior bounce improvement on Sponza; no validation spam.
- Benchmark: DDGI on/off delta row in PR notes.

<a id="validation-s9"></a>
## S9 validation (simulation)

*Former §S8 simulation.*

- Fixed-step simulation updates SoA data before extract.
- Physics + animation + AI minimum acceptance slice is demonstrated.
- Gameplay-facing debug overlays/logs show expected state transitions.

<a id="validation-s10"></a>
## S10 validation (M3 — meshlets)

*Former §S4 meshlet. **Gate G3** (MeshImport v0).*

- Meshlet assets load and render for at least one production mesh.
- Meshlet debug visualization is available and correct.
- Meshlet metadata/bounds buffers are validated in capture.

<a id="validation-s11"></a>
## S11 validation (M4 — mesh shader)

*Former §S5.*

- Mesh shader feature probe and fallback behavior are correct.
- Mesh shader path matches VS path for **G-buffer + deferred** parity (agreed tolerance).
- RenderDoc/validation capture checklist passes.

<a id="validation-s12"></a>
## S12 validation (M5 — GPU mesh tasks)

*Former §S6.*

- GPU meshlet cull + `vkCmdDrawMeshTasksIndirectEXT` path is active.
- CPU record cost remains stable with instance count growth.
- Preset fallback matrix (`Traditional` / `GpuIndirect` / `MeshShader` / `FullGpuMesh`) works.

<a id="validation-s13"></a>
## S13 validation (render lab infra)

*Deferred M6 infra (WSI, codegen, docs). Does not block G4.*

- WSI maintenance1 path probed when extension present (or documented N/A).
- At least one infra deliverable from Wishlist §S13 closed with evidence.

---

## Evidence checklist (all sprints)

- Build result (command + exit code)
- Smoke-run logs (`Logs/engine_runtime_log.txt`)
- Visual evidence (screenshots or capture notes)
- Regression statement vs previous sprint baseline
