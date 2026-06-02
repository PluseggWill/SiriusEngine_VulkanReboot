# Progress: lod-v0

## 2026-05-26 — Plan created

- **Plan ref:** Initial plan from `SprintPlan.md` S1 LOD v0 block.
- **Files:** `Docs/lod-v0_Plan.md`, `Docs/lod-v0_Progress.md`
- **What changed:** Task opened.
- **Verification:** N/A

## 2026-05-26 — LOD table + SoA columns

- **Plan ref:** Goals 2–4; Files `Gfx_Lod.*`, `Gfx_SceneSoA.*`
- **Files:** `VulkanDesktop/Gfx/Gfx_Lod.h`, `Gfx_Lod.cpp`, `Gfx_SceneSoA.h`, `Gfx_SceneSoA.cpp`, `Gfx_DrawExtract.cpp`, `Util_DemoAssets.h`, `Data/LOD.md`
- **What changed:** `logicalMeshId` + `lodBias` on SoA; `Gfx_LodTable` / hysteresis resolver; demo tree chain detailed→simple @ 14 m.
- **Verification:** MSBuild Debug|x64 — pending full build in next step.

## 2026-05-26 — Demo hook + docs

- **Plan ref:** Goals 5–6; Verification
- **Files:** `Vk_Core.cpp`, `Vk_Core.h`, `VulkanDesktop.vcxproj`, `.filters`, `Docs/EngineArchitecture.md`, `Docs/SprintPlan.md`, `Docs/README.md`, `Data/ASSETS.md`
- **What changed:** Apply LOD after cull; two demo trees on `kLogicalTree` (near/far); `[LOD]` log with `resolvedMeshId` + distance; sprint archived.
- **Verification:** MSBuild Debug|x64 exit 0; smoke-run 4 s — `[LOD]` near tree `resolvedMeshId=2`, far tree `resolvedMeshId=3`; `entities=9 draws=9`.

## 2026-05-26 — Doc sync (pre-commit)

- **Plan ref:** Goal 6; review pass
- **Files:** `Docs/EngineArchitecture.md`, `Data/LOD.md`, `Docs/SprintPlan.md` (M1 LOD acceptance)
- **What changed:** Render path (today) lists LOD step; `Data/LOD.md` hysteresis + verification; M1 LOD checkbox marked done.
- **Verification:** N/A (docs only)
