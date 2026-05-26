# Progress: descriptor-strategy

## 2026-05-22 — Research + plan

- **Plan ref:** Problem, research verdict, locked policy
- **Files:** `Docs/descriptor-strategy_Plan.md`
- **What changed:** Codebase audit; validated tiered static+dynamic+push hybrid; documented demo vs target gaps (env “dynamic” comment mismatch, model in camera UBO).
- **Verification:** Manual read of `Vk_Core.cpp`, shaders, `EngineArchitecture.md`

## 2026-05-22 — P1–P4 policy lock (docs + code contracts)

- **Plan ref:** P1–P4
- **Files:** `Docs/EngineArchitecture.md`, `VulkanDesktop/RenderCore/Vk_DescriptorPolicy.h`, `Vk_Enum.h`, `Vk_FrameData.h`, `Vk_Camera.h`, `Vk_Core.cpp`
- **What changed:** §5.3 expanded with locked hybrid policy; new policy header; set indices; comment fixes; demo `model` called out.
- **Verification:** `msbuild` not on PATH in agent shell; user should rebuild Debug x64 in VS. Grep: no `UNIFORM_BUFFER_DYNAMIC` in VulkanDesktop sources (policy/docs only).

## 2026-05-22 20:00 — P5 build verify

- **Plan ref:** P5
- **Files:** (none changed)
- **What changed:** n/a
- **Verification:** `msbuild VulkanDesktop.vcxproj /p:Configuration=Debug /p:Platform=x64` — **exit 0**; output `x64\Debug\VulkanDesktop.exe`. Shaders recompiled via glslc. Pre-existing `LNK4098` (MSVCRT defaultlib) warning only.

## 2026-05-22 — P6 archive + S1 verification hooks

- **Plan ref:** P6
- **Files:** `Docs/SprintPlan.md`, `Docs/descriptor-strategy_Plan.md`, `Vk_*.h/cpp`, `TriangleVertex.vert`
- **What changed:** S0 task moved to Archived; S1/S2/M1 verification bullets added; `TODO(descriptor-strategy)` at unwired Set 1/2/push sites.
- **Verification:** SprintPlan hygiene (no `[x]` in active S0); grep `TODO(descriptor-strategy)` in VulkanDesktop.
