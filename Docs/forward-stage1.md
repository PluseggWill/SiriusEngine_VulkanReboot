# Stage 1 forward baseline & Stage 2 handoff

**Status:** Closed (2026-06-02) — epic [`Archived/plans/forward-rendering-epic_Plan.md`](Archived/plans/forward-rendering-epic_Plan.md) §A–C + Acceptance.  
**Next (shipped):** [`Archived/plans/hybrid-deferred-epic_Plan.md`](Archived/plans/hybrid-deferred-epic_Plan.md) — Stage 2 + G4 accepted.

| Item | Value |
|------|--------|
| Preset | `ForwardLit` → permutation `lit` |
| Scene | [`Data/Scenes/demo.json`](../Data/Scenes/demo.json) (8 opaque + 1 transparent draws) |
| Config | [`Config/engine.benchmark.json`](../Config/engine.benchmark.json) |
| Golden | [`Assets/golden/forward-stage1/forward-lit-demo-1600x1200.png`](Assets/golden/forward-stage1/forward-lit-demo-1600x1200.png) |

Archived task logs: `forward-stage1-contracts`, `forward-pass-hardening`, `forward-stage1-validation` under [`Archived/plans/`](Archived/plans/).

---

## 1. Baseline capture runbook

### Prerequisites

- Debug\|x64 build: `x64\Debug\VulkanDesktop.exe`
- `assetVerify: strict`
- **Do not move the fly camera** after startup (default eye `(2, 2, 2)` → center `(0, 0, 0)`, Z-up — `Vk_Camera` constructor)

### Launch

```powershell
Set-Location x64\Debug
.\VulkanDesktop.exe --asset-root <repo> --config <repo>\Config\engine.benchmark.json --no-validation
```

CLI equivalent: `--no-demo-rotate --render-preset ForwardLit --scene Data/Scenes/demo.json`.

**ImGui:** Render Debug = Lit; do not skip opaque/transparent for baseline capture.

### Golden screenshot

1. Launch; wait ~2 s for scene visible.
2. Window 1600×1200, unobstructed.
3. Capture client area; compare to reference PNG above.

**Expect:** Viking room + Kenney camp; transparent monkey overlay; no validation overlay.  
**Note:** Reference is **not** pixel CI (GPU / window placement may differ).

**Reference metadata:** captured 2026-06-02 on NVIDIA GeForce RTX 4070 Ti SUPER, Windows 1600×1200.

### Performance baseline

Run ≥2 s; read first `[PERF]` in `Logs/engine_runtime_log.txt`.

| Field | Value (2026-06-02 sample) |
|-------|---------------------------|
| GPU | NVIDIA GeForce RTX 4070 Ti SUPER |
| Resolution | 1600×1200 |
| Vsync | Sample row used **display cap** (`engine.json` default). **`Config/engine.benchmark.json` uses `vsync: false`** for uncapped perf / `--perf-log`. |
| `frameMs` / `fps` | 16.57 / 60.35 (vsync-on sample) |
| `visibleDraws` | 9 |
| `batchRuns` | 9 (opaque 8 + transparent 1) |
| `materialPath` | Batch |

```text
[PERF] frameMs=16.568899 fps=60.354038 entities=9 visibleDraws=9 batchRuns=9 (opaque=8 transparent=1) materialSetBinds=9 pipelineBinds=9 drawCalls=9 materialPath=Batch
```

Formal CSV / GPU timestamps → **S7** benchmark harness (Sponza default scene).

### Smoke (CI / task close)

```powershell
.\VulkanDesktop.exe --asset-root <repo> --config <repo>\Config\engine.benchmark.json --no-validation --smoke-seconds 6
```

Exit **0**; log: `renderPreset=ForwardLit`, `LoadSceneResources completed`, `Smoke dwell reached`.

---

## 2. Stage 2 handoff checklist

Use before starting hybrid deferred work.

### Baseline evidence

- [ ] §1 runbook followed on target machine
- [ ] Golden compared to reference PNG
- [ ] `[PERF]` row updated in §1 for your GPU if materially different

### Contracts to preserve

| Topic | Source of truth |
|-------|-----------------|
| Material CPU/GPU layout | `GpuMaterialParams`, `GpuMaterialTableEntry`, `Vk_Types.h`; `SceneJSON.md` |
| Descriptor sets 0/1/2 | `Vk_DescriptorPolicy.h`, `EngineArchitecture.md` §6.1 |
| Draw stream | `Gfx_FrameRenderPacket` opaque → transparent |
| Transparent policy | `EngineArchitecture.md` §5.2 — back-to-front sort, depth write off on transparent pipe |
| Stage 2 depth import | §5.2 — `ForwardTransparent` reads opaque depth; test on / write off |
| Render preset | `ForwardLit` via `Gfx_RenderPreset`, config / CLI |
| Shader perm bits | `Gfx_ShaderFeatureBit`, `PermutationRegistry.json` |
| Pass record | `Vk_ScenePasses` CONTRACT (single RP, two sub-stages) |

### Debug / parity (keep working)

- ImGui **Render Debug** — skip passes, Lit / Depth / World normal
- `ForwardLit` remains A/B fallback after `HybridDeferred`

### Stage 2 entry (summary)

1. Accept §3 gaps — do not re-scope Stage 1.
2. Pass chain: `GBufferOpaque → ClusterBuild → DeferredLighting → ForwardTransparent → Post`.
3. Opaque deferred/clustered PBR; transparent forward over imported depth.
4. Preset `HybridDeferred`; parity vs `demo.json` using §1 runbook.

---

## 3. Known gaps (deferred to Stage 2+)

Not blockers if §2 is satisfied.

### Lighting / shading

| Gap | Target | Notes |
|-----|--------|-------|
| PBR BRDF | **S4** ✓ | Cook-Torrance direct sun; MR in G-buffer RT0.a / RT1.w |
| IBL / shadows | **S5** | Perm bits exist; implementation S5 |
| `HybridDeferred` preset | S3 ✓ | Default in `engine.json`; forward fallback kept |
| Clustered deferred | S3 ✓ stub | ClusterBuild + DeferredLighting v0 landed |

### Pipeline / passes

| Gap | Target | Notes |
|-----|--------|-------|
| Frame graph | **S7** | S3 FG v0 = handwritten chain; full FG builder S7 |
| `ForwardTransparent` FG node | S3 ✓ | Over deferred depth; FG formalization S7 |
| GPU indirect | S3 ✓ | G1 parity; `--gpu-cull` dogfood |
| Mesh shader / meshlets | **S10–S12** | Geometry track (deferred) |

### Materials / alpha

| Gap | Target | Notes |
|-----|--------|-------|
| `alphaMode=blend` in shader | Stage 2 | Pipeline blend today |
| mask vs global `ALPHA_CLIP` | Stage 2+ | Both exist |
| Bindless default | Optional | Batch on baseline machine |

### Tooling

| Gap | Target | Notes |
|-----|--------|-------|
| Golden pixel CI | S7 | Manual PNG + §1 |
| p50/p95 CSV, GPU timestamps | S7 | Benchmark runbook on Sponza |
| Runtime preset hot-reload | S7 / Backlog | Restart / CLI |
| Multi-view / `cameras[]` | S2 | See archived `multi-view` plan |
| Dedicated benchmark scene pack | S4+ | `sponza.json` default; `stress.json` for CI |

---

## Related

- Architecture: [`EngineArchitecture.md`](EngineArchitecture.md) §5.2, §7
- Validation gate: [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) § Stage 1 forward
- CLI / config: [`CLI.md`](CLI.md)
