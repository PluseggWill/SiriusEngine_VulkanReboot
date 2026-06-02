# Plan: ci-verification

**Status:** Planned (roadmap infra — no WIP Progress until implementation starts)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P0**  
**Covers recommendations:** #2, #16, #20, #22, #25, #26, #27, #32

## Goal

Replace “6s smoke = done” with **reproducible build + correctness + perf signals** before M2 and Stage 2 work lands.

## Non-goals

- Full pixel golden CI (manual PNG stays until S7 harness)
- Cross-platform CI matrix

---

## Work breakdown

### A. GitHub Actions — build + shader compile (#2)

**Landing:** Workflow on push/PR: MSBuild Debug\|x64 + `CompileShader_Glslc.bat`; fail if SPIR-V or reflection JSON drift.

| Step | Done when |
|------|-----------|
| Add `.github/workflows/vulkan-desktop.yml` | Green on clean checkout |
| Cache nothing critical v1 | Shader outputs regenerated each run |

### B. Asset root — fail fast (#20)

**Landing:** Startup requires resolved `assetRoot` (CLI `--asset-root`, exe-adjacent marker, or non-empty `engine.json`); log `[CONFIG] assetRoot=`; **no silent cwd walk** in release path.

| Step | Touch |
|------|-------|
| `Util_EngineConfig::Initialize` | Remove or gate `FindRepoRoot()` fallback behind `DEV_ALLOW_CWD_DISCOVERY=1` |
| `Docs/CLI.md` | Document required invocations |

### C. Smoke upgrade (#25)

**Landing:** CI job runs `--no-validation --smoke-seconds 6` (or `--smoke-frames N`) with explicit `--asset-root`; assert exit 0 + log tokens from `vulkan-smoke-test.mdc`.

Optional follow-up: **headless/offscreen** one frame (GLFW HIDDEN or skip present) — separate job if flaky on runners.

### D. Structured perf log (#16, #32)

**Landing:**

- Benchmark config: **`vsync: false`** in `engine.benchmark.json` (verify default demo config documents vsync-on = display cap).
- Optional `--perf-log path.jsonl`: one JSON object per frame (`frameMs`, `drawCalls`, `visibleDraws`, `materialPath`).
- Keep `[PERF]` first-frame log for humans; JSONL for regression scripts.

### E. Automated tests — v0 (#22, #25)

**Landing:** Small test binary or `VulkanDesktop.exe --run-tests` (no GPU):

| Test | Scope |
|------|-------|
| SoA generation | Alloc → free → reuse slot → `IsAlive` false for stale id |
| Extract/cull | Fixed SoA + camera → golden visible count (CPU path) |
| Sort/batch | Known draws → batch run count |

GPU cull parity (#25 extension): fixed scene + camera JSON → compare draw count CPU vs GPU (after M2).

### F. Sprint closeout discipline (#26, #27)

**Landing:**

- [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md): add **adversarial row** — pick one archived claim, verify in code or log.
- Completion metrics for peel tasks: e.g. `DrawFrame` LOC, `Vk_Core` LOC, count of `friend class` — not task checkbox count.

---

## Verification

- CI green on PR touching `VulkanDesktop/` or `Shader/`
- Local: `.\VulkanDesktop.exe --asset-root <repo> --config Config\engine.benchmark.json --no-validation --smoke-seconds 6`
- Perf: JSONL 300 frames, p50 frameMs logged by script

## Risks

- GPU runners unavailable → keep compile + `--run-tests` on CPU-only agents; smoke on manual/nightly.
