# Plan: gfx-vk-decoupling

**Sprint:** S2 - Engine layering & hygiene  
**Status:** Closed (2026-05-28, archived in `SprintPlan.md`)  
**Related:** [`SprintPlan.md`](SprintPlan.md) (S2 boundary hardening), [`EngineArchitecture.md`](EngineArchitecture.md) §3, §4, §9, [`vk-core-decomposition_Plan.md`](vk-core-decomposition_Plan.md)

## Problem

Current RenderCore still directly consumes multiple `Gfx_*` types and semantics during record/submit preparation. This weakens the intended direction (`Gfx -> backend contract -> Vk`) and makes backend replacement, module testing, and boundary ownership harder to maintain.

## Goals

1. Establish a backend-facing render packet contract so `Gfx` is the producer and `Vk` is a pure consumer.
2. Remove direct `Gfx_*` semantic ownership from `Vk_*` execution path (especially `Vk_Core`, `Vk_FrameDrawPrep`, `Vk_ScenePasses`).
3. Keep frame behavior parity during migration (draw counts, sort/batch behavior, descriptor usage, instance slab writes).
4. Preserve existing fallback/material path behavior (batch/bindless) while moving ownership boundaries.

## Non-goals

- Rewriting shader contracts, descriptor policy, or material system policy.
- Introducing frame graph in this task.
- Changing scene JSON format or scene-load Phase D scope.
- Removing all `Gfx_*` symbols in one commit without phased verification.

## Design decisions

| Topic | Decision |
|------|----------|
| Boundary object | Introduce backend-agnostic packet (`FrameRenderPacket` + per-pass draw packets) produced in `Gfx` |
| Backend API | Introduce RenderCore-facing backend contract (`Vk_RenderBackend` module) that consumes packets only |
| Migration mode | Two-phase migration: dual-path verification first, then full switch |
| Ownership | `Gfx`: extract/cull/LOD/sort/batch semantics. `Vk`: upload/bind/record/submit only |
| Safety | Keep log parity checks and smoke-run per phase |

## File touch list (complete)

### Docs

- `Docs/SprintPlan.md`
- `Docs/EngineArchitecture.md`
- `Docs/gfx-vk-decoupling_Plan.md`
- `Docs/gfx-vk-decoupling_Progress.md`

### New bridge / contract layer

- `VulkanDesktop/Gfx/Gfx_RenderPacket.h`
- `VulkanDesktop/Gfx/Gfx_RenderPacket.cpp` *(if builder helpers are non-trivial)*
- `VulkanDesktop/RenderCore/Vk_RenderBackend.h`
- `VulkanDesktop/RenderCore/Vk_RenderBackend.cpp`

### Gfx producers (packet build side)

- `VulkanDesktop/Gfx/Gfx_FrameDrawStream.h`
- `VulkanDesktop/Gfx/Gfx_FrameDrawStream.cpp`
- `VulkanDesktop/Gfx/Gfx_DrawExtract.h`
- `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp`
- `VulkanDesktop/Gfx/Gfx_DrawBatch.h`
- `VulkanDesktop/Gfx/Gfx_DrawBatch.cpp`

### RenderCore consumers (packet consume side)

- `VulkanDesktop/RenderCore/Vk_Core.h`
- `VulkanDesktop/RenderCore/Vk_Core.cpp`
- `VulkanDesktop/RenderCore/Vk_FrameDrawPrep.h`
- `VulkanDesktop/RenderCore/Vk_FrameDrawPrep.cpp`
- `VulkanDesktop/RenderCore/Vk_ScenePasses.h`
- `VulkanDesktop/RenderCore/Vk_ScenePasses.cpp`
- `VulkanDesktop/RenderCore/Vk_DescriptorSystem.h` *(if consume API surface changes)*
- `VulkanDesktop/RenderCore/Vk_DescriptorSystem.cpp` *(if consume API surface changes)*
- `VulkanDesktop/RenderCore/Vk_ResourceTables.h`
- `VulkanDesktop/RenderCore/Vk_ResourceTables.cpp`

### App orchestration

- `VulkanDesktop/App/Application.cpp` *(if build/consume handoff timing shifts)*

### Project files

- `VulkanDesktop/VulkanDesktop.vcxproj`
- `VulkanDesktop/VulkanDesktop.vcxproj.filters`

## Implementation plan

### P1 - Contract definition (no behavior change)

- [x] Add `Gfx_RenderPacket` plain-data contract for frame/pass draw data.
- [x] Add `Vk_RenderBackend` consume interface in RenderCore.
- [x] Wire compile paths (`.vcxproj` / `.filters`) and keep old path fully active.

### P2 - Producer/consumer dual-path (verification phase)

- [x] Build packet from existing Gfx extract/batch output.
- [x] Feed packet into RenderCore in parallel (shadow path) and compare key counters/logs.
- [x] Confirm parity signals: visible draws, batch runs, instance slab count, draw calls.

### P3 - Switch execution to packet-only consume path

- [x] Make `Vk_Core`/`Vk_FrameDrawPrep`/`Vk_ScenePasses` consume packet contract as primary path.
- [x] Move remaining Gfx semantic decisions out of `Vk_*` path.
- [x] Keep descriptor/pipeline behavior unchanged (batch/bindless parity).

### P4 - Coupling cleanup and guardrails

- [x] Remove obsolete direct `Gfx_*` dependency points from RenderCore execution path.
- [x] Add light guardrail checks (code comments/log asserts) for boundary invariants.
- [x] Update docs with final boundary wording and accepted constraints.

## Closeout checklist

- [x] RenderCore runtime consume path is packet-only.
- [x] `RenderCore/` no longer references `Gfx_ExtractResult`.
- [x] `Debug|x64` build + smoke-run pass on latest decoupling batch.
- [x] Optional final cleanup commit: remove now-unused compatibility logs/toggles and lock boundary comments.

## Closeout

- Task archived from active S2 queue into `SprintPlan.md` `## Archived` on 2026-05-28.
- Architecture wording synchronized in `EngineArchitecture.md` (`§3.1`, `§9`, footer).

## Build / smoke-run plan

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
& "<MSBuild.exe>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
Set-Location x64\Debug
$p = Start-Process -FilePath ".\VulkanDesktop.exe" -PassThru -WindowStyle Minimized
Start-Sleep -Seconds 4
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
```

Log checks:

- No new `[ERROR]` in init/render path.
- `EXTRACT`, `BATCH`, `FillInstanceSlab`, `PERF` signals still present.
- Draw/path counts remain within expected parity for `Data/Scenes/demo.json`.

## Acceptance criteria

| # | Criterion |
|---|-----------|
| A1 | `Gfx` produces explicit frame/pass packet consumed by RenderCore |
| A2 | `Vk` execution modules no longer own Gfx-side semantic decisions |
| A3 | Runtime parity preserved (draw count/batch count/instance slab path) |
| A4 | Docs (`SprintPlan` + `EngineArchitecture`) remain aligned on boundary intent |

## Risks and rollback

| Risk | Mitigation |
|------|------------|
| Packet shape drifts from current draw path | Dual-path parity phase before hard switch |
| Hidden coupling remains in utility calls | Phase P4 grep + targeted cleanup checklist |
| Behavior regressions in batch/bindless modes | Keep descriptor/pipeline contract unchanged; verify both modes in smoke logs |

Rollback:

- Revert per phase (P1/P2/P3/P4) commits independently.
- Keep old path guarded until P3 parity sign-off.
