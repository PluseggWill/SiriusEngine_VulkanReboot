# Sprint Outcome Validation

Validation runbook for sprint close-out. Use with [`Active-Plan.md`](Active-Plan.md) (open tasks) and [`Archived-Plan.md`](Archived-Plan.md) (completed lines).

## How to use

1. Finish all planned sprint tasks or explicitly defer remaining work.
2. Run the sprint-specific validation checklist below.
3. Record outputs (logs/screenshots/captures) in PR or progress notes.
4. Only then move completed sprint lines from **`Active-Plan.md`** → **`Archived-Plan.md`**.

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
- [`forward-stage1.md`](forward-stage1.md) §2 handoff reviewed and §3 gaps accepted before Stage 2.
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
## S3 validation (M2)

- GPU frustum cull -> indexed indirect path is active.
- Draw count comes from GPU outputs (not per-object CPU draw loop).
- Fixed-camera parity check against CPU cull is within agreed tolerance.

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
## S4 validation (M3)

- Meshlet assets load and render for at least one production mesh.
- Meshlet debug visualization is available and correct.
- Meshlet metadata/bounds buffers are validated in capture.

<a id="validation-s5"></a>
## S5 validation (M4)

- Mesh shader feature probe and fallback behavior are correct.
- Mesh shader path matches VS path for agreed geometry/pass-contract parity.
- RenderDoc/validation capture checklist passes.

<a id="validation-s6"></a>
## S6 validation (M5)

- GPU meshlet cull + `vkCmdDrawMeshTasksIndirectEXT` path is active.
- CPU record cost remains stable with instance count growth.
- Preset fallback matrix (`Traditional` / `GpuIndirect` / `MeshShader` / `FullGpuMesh`) works.

<a id="validation-s7"></a>
## S7 validation (M6)

- Frame graph drives expected pass chain for active preset.
- Preset/permutation switching is validation-clean.
- Benchmark runbook produces reproducible outputs (p50/p95, warm/cold notes).
- At least one extra pass (shadow/post/tonemap) is verified in capture.

<a id="validation-s8"></a>
## S8 validation

- Fixed-step simulation updates SoA data before extract.
- Physics + animation + AI minimum acceptance slice is demonstrated.
- Gameplay-facing debug overlays/logs show expected state transitions.

---

## Evidence checklist (all sprints)

- Build result (command + exit code)
- Smoke-run logs (`Logs/engine_runtime_log.txt`)
- Visual evidence (screenshots or capture notes)
- Regression statement vs previous sprint baseline
