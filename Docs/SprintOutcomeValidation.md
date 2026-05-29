# Sprint Outcome Validation

Validation runbook for sprint close-out. Use with [`Active-Plan.md`](Active-Plan.md) (open tasks) and [`Archived-Plan.md`](Archived-Plan.md) (completed lines).

## How to use

1. Finish all planned sprint tasks or explicitly defer remaining work.
2. Run the sprint-specific validation checklist below.
3. Record outputs (logs/screenshots/captures) in PR or progress notes.
4. Only then move completed sprint lines from **`Active-Plan.md`** → **`Archived-Plan.md`**.

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

<a id="validation-s2"></a>
## S2 validation

- Application lifecycle path runs cleanly: init -> update/render loop -> unload -> shutdown.
- `Util_EngineConfig` and CLI overrides behave as expected.
- Scene-load phase gates (A/B/C/D as applicable) match current sprint state.
- `Vk_Core` peel milestones compile/smoke pass without behavior regression.

<a id="validation-s3"></a>
## S3 validation (M2)

- GPU frustum cull -> indexed indirect path is active.
- Draw count comes from GPU outputs (not per-object CPU draw loop).
- Fixed-camera parity check against CPU cull is within agreed tolerance.

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
