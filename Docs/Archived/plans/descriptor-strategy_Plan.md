# Plan: descriptor-strategy

**Sprint:** S0 — Foundation & tooling  
**Status:** **Archived** (2026-05-22) — policy locked; runtime proof deferred to S1/S2 tasks in `Docs/SprintPlan.md`

## Problem

`EngineArchitecture.md` §5.3 only compares bindless vs traditional; it does not decide **`UNIFORM_BUFFER` vs `UNIFORM_BUFFER_DYNAMIC`**, set frequency layout, or push-constant rules. The live demo path mixes **per-frame camera**, **per-frame env**, and **per-object `model`** in one `GpuCameraData` UBO (binding 0), which will not scale to S1 multi-draw. Comments in `Vk_Core` refer to “dynamic offset” while bindings use **static** `VkDescriptorBufferInfo::offset` (not `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`).

S0 must **lock policy** in architecture + code contracts so S1 draw-stream work does not churn descriptors again.

## Research verdict (user proposal)

| Claim | Verdict | Notes |
|-------|---------|--------|
| Frequency-tiered sets (0=global, 1=material, 2=object) | **Valid** | Industry-standard; matches §5.1 batching and §4.4 instance slabs. Set numbers are convention. |
| Global/frame data → `UNIFORM_BUFFER` on set 0 | **Valid** | Prefer **one UBO + one descriptor set per in-flight frame** (current camera path) to avoid CPU/GPU races. |
| Per-object data → `UNIFORM_BUFFER_DYNAMIC` on set 2 into a ring/slab | **Valid** | One bind of set 2 per batch; `vkCmdBindDescriptorSets(..., dynamicOffset)` per draw/instance. Requires dynamic type in **layout** and pool. |
| Small per-draw data → push constants (e.g. ≤128 B) | **Valid with limits** | **Model `mat4` only (64 B)** fits everywhere. **Full** `GpuCameraData` (192 B) does **not** fit typical `minPushConstantsSize` (128); do not put view/proj/model in push constants together without checking `maxPushConstantsSize`. |
| “Combine both UBO kinds” | **Correct** | Not either/or: static UBO for rare binds, dynamic UBO for high-frequency offsets, push constants for smallest per-draw fields. |

**Not valid as stated today:** putting **camera + model** in one binding and calling it “global” — `model` updates per object/draw; it belongs in set 2 or push constants in S1.

**Current code nuance:** env data uses a **single buffer** with **frame-indexed static offsets** baked into `vkUpdateDescriptorSets` at init — this is **not** `UNIFORM_BUFFER_DYNAMIC`. Acceptable for per-frame env; rename comments to avoid confusion.

## Goals (S0)

1. Document a **locked hybrid descriptor policy** in `EngineArchitecture.md` §5.3 (expand section; keep bindless as S1 fork).
2. Add a **single code contract** (`Vk_DescriptorPolicy.h` + `Vk_Enum.h` set indices) aligned with shaders **when** they change in S1.
3. Clarify **today’s demo** as a temporary layout: set 0 only; `model` in camera UBO is demo-only.
4. Fix misleading “dynamic UBO” comments; document `PadUniformBufferSize` purpose (slab stride + future dynamic offsets).

## Non-goals (S0)

- Multi-set pipeline layout, new descriptor pools, or shader splits (S1).
- Wiring `myObjectBuffer` / `myObjectDescriptor` to the draw path.
- Bindless / descriptor indexing decision (S1 task in `SprintPlan.md`).
- Refactoring `TriangleVertex.vert` to split `model` from camera (S1).

## Locked policy (v1)

### Descriptor sets by update frequency

| Set | Name | Update rate | Types | CPU/GPU pattern |
|-----|------|-------------|-------|-----------------|
| **0** | `Frame` | Once per in-flight frame | `UNIFORM_BUFFER` (camera view/proj), `UNIFORM_BUFFER` (env), `COMBINED_IMAGE_SAMPLER` (per material batch later) | One descriptor set per `MAX_FRAMES_IN_FLIGHT`; memcpy each frame; **no** `vkUpdateDescriptorSets` on hot path. |
| **1** | `Material` | Per batch / material table | `UNIFORM_BUFFER` and/or `COMBINED_IMAGE_SAMPLER` (bindless SSBO deferred S1) | Bind once per material batch in S1. |
| **2** | `Object` | Per draw / instance | `UNIFORM_BUFFER_DYNAMIC` → **instance ring slab**; optional **push constants** for `mat4 model` only | One set bind per batch; `dynamicOffset[]` per draw; slab stride = `PadUniformBufferSize(sizeof(GpuObjectData))`. |

### `UNIFORM_BUFFER` vs `UNIFORM_BUFFER_DYNAMIC` (decision)

| Use | Type | Rationale |
|-----|------|-----------|
| Frame camera (view, proj), env, material tables | `UNIFORM_BUFFER` | Bound once per frame or batch; offset fixed at alloc or batch boundary. |
| Instance records in a large per-frame slab | `UNIFORM_BUFFER_DYNAMIC` | Many draws share one descriptor; only `dynamicOffset` changes — matches validation-friendly batching. |
| Single `mat4` model matrix | **Push constants** (preferred for VS path S1) OR dynamic UBO if push budget tight | 64 B; avoids descriptor churn for simplest draws. |
| Full 192 B `GpuCameraData` in push constants | **Disallowed** without capability check | Exceeds 128 B minimum limit on many devices. |

### Demo path (until S1)

- Keep **one** bound set (set 0) for the triangle/OBJ demo.
- `GpuCameraData.model` remains for `ENABLE_ROTATE` demo only; documented as **non-architectural**.
- Env: keep frame-indexed **static** offsets in one `myEnvDataBuffer`; do not label as dynamic descriptor type.

### Alignment

- All slab strides and dynamic offsets: `Vk_Core::PadUniformBufferSize` (= `minUniformBufferOffsetAlignment`).
- Structs: `std140` for UBOs; sizes asserted in debug where practical (S1).

## Files (touch list)

| File | Change |
|------|--------|
| `Docs/EngineArchitecture.md` | Replace/expand §5.3 with locked policy + diagram |
| `Docs/descriptor-strategy_Plan.md` | This file |
| `Docs/descriptor-strategy_Progress.md` | Implementation log |
| `VulkanDesktop/RenderCore/Vk_DescriptorPolicy.h` | **New** — policy constants + comments |
| `VulkanDesktop/RenderCore/Vk_Enum.h` | Set indices; binding comments |
| `VulkanDesktop/RenderCore/Vk_FrameData.h` | Set 2 reservation comments |
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | Comment fixes; include policy header |
| `VulkanDesktop/VulkanDesktop.vcxproj` + `.filters` | Add new header |

## Implementation steps

- [x] **P1** — `EngineArchitecture.md` §5.3: frequency tiers, UBO vs dynamic vs push, demo exception, S1 hooks.
- [x] **P2** — Add `Vk_DescriptorPolicy.h`; extend `Vk_Enum.h` with set indices (`eVk_SetFrame`, etc.).
- [x] **P3** — Comment pass: `Vk_Core.cpp` (env slab, `PadUniformBufferSize`), `Vk_FrameData.h`, `Vk_Camera.h` (split note for `model`).
- [x] **P4** — Register header in VS project/filters.
- [x] **P5** — Manual: project builds; grep shows no false “UNIFORM_BUFFER_DYNAMIC” claims in set 0 path.
- [x] **P6** — Move S0 task to `SprintPlan.md` → Archived; S1/S2 verification lines added.

## Verification

1. **Build** `VulkanDesktop` (Debug) — no compile errors.
2. **Run** demo — single mesh still renders; camera/env unchanged.
3. **Review** `EngineArchitecture.md` §5.3 and `Vk_DescriptorPolicy.h` read as one consistent story.
4. **Grep** `UNIFORM_BUFFER_DYNAMIC` — only in policy/architecture (or explicit “S1 TODO”), not falsely on set 0 env binding.

## Risks

| Risk | Mitigation |
|------|------------|
| S1 shader split breaks demo | Plan explicit S1 step; demo flag until draw stream lands |
| Push constant size on Intel iGPU | Use `mat4` only; query `maxPushConstantsSize` before larger blocks |
| Over-scoping S0 | No new sets/pools/shaders in S0 |

## Open choices (defaults if silent)

| Topic | Default |
|-------|---------|
| Per-draw transform in S1 | Push constant `mat4 model` first; dynamic UBO for extra per-instance fields |
| Bindless | Deferred to S1 § “Bindless vs batch+push” |
