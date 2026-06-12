# Plan: s4-pbr-gbuffer — PBR material contract + deferred BRDF (Lighting-1)

**Status:** Closed (2026-06-12)  
**Progress:** [`s4-pbr-gbuffer_Progress.md`](s4-pbr-gbuffer_Progress.md) *(same folder)*  
**Parent:** [`Active-Plan.md`](Active-Plan.md) queue #1 · [`Wishlist.md`](Wishlist.md) § S4  
**Epic:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §B–C (partial — direct sun only, no IBL)  
**Depends on:** S3 FG v0 ✓  
**Validation runbook:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) § S4

---

## 0. Agent quick start (zero prior context)

You are implementing **S4** in repo `SiriusEngine_VulkanReboot` (VulkanDesktop, Windows, MSBuild Debug|x64).

**Read first (5 min):** this plan §1–§4, then open the files in §3 **Current state**.

**Do not change without explicit plan step:** descriptor Set 0/1/2 layouts, G-buffer Vulkan formats, `ClusterBuild.comp`, transparent forward shaders, `PermutationRegistry.json`.

**After every shader edit:** run `VulkanDesktop\Shader\CompileShader.bat` from repo root, **commit** updated `VulkanDesktop/Shader_Generated/*.spv`. CI runs `Assert-ShaderDrift.ps1` — uncommitted `.spv` fails G0.

**Verify after each step:** `powershell -File Scripts/Verify-CI.ps1` (minimum). GPU/runtime steps also run `Verify-Smoke.ps1`.

**Log progress:** append a checkpoint to [`s4-pbr-gbuffer_Progress.md`](s4-pbr-gbuffer_Progress.md) (template §12).

---

## 1. Problem

S3 landed hybrid FG v0 with **Blinn-Phong specular v0**. `roughness` / `metallic` exist in CPU + material UBO/bindless table (Stage 1 contract) but:

| Gap | Location today |
|-----|----------------|
| Metallic not written to G-buffer | `GBuffer.frag` L36 `outAlbedo = vec4(albedo, 1.0)` |
| Deferred ignores MR | `DeferredLighting.frag` L63–89 — reads `albedo.rgb` only; spec uses `pc.specParams.x/y` shininess |
| Forward ignores MR | `TriangleFrag_Lit.frag` L52–69 — same Blinn-Phong + env tunables |

**User-visible symptom:** `HybridDeferred` on Sponza looks like flat diffuse + fixed highlight size, not material-driven PBR.

---

## 2. Goals (definition of done)

| ID | Deliverable | Objective test |
|----|-------------|----------------|
| G1 | G-buffer MR encode | RT0.a = metallic, RT1.w = roughness; batch + bindless paths |
| G2 | Cook-Torrance deferred | `DeferredLighting` GGX + Schlick + Smith; sun from cluster SSBO |
| G3 | Forward direct PBR | `TriangleFrag_Lit*` same BRDF; material UBO MR consumed |
| G4 | Cluster sun | No structural change; `WriteSunLightFromEnvironment` still feeds light `[0]` |
| G5 | Sponza MR diversity | Scene materials have **varied** roughness (not all default 0.5) |
| G6 | CI | `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 |

---

## 3. Current state snapshot

### 3.1 Frame graph (HybridDeferred — default `Config/engine.json`)

```
GBufferOpaque → ClusterBuild → DeferredLighting → [depth copy] → ForwardTransparent → present
```

- Orchestration: `VulkanDesktop/RenderCore/Vk_GBufferPass.cpp` (`RecordFrame`)
- Preset gate: `Gfx_RenderPreset::IsHybridDeferred` in `Vk_ScenePasses.cpp`
- Default config: `Config/engine.json` → `"renderPreset": "HybridDeferred"`, `"scene": "Data/Scenes/sponza.json"`

### 3.2 G-buffer resources (unchanged in S4)

| RT | C++ constant | Format | Shader output today |
|----|--------------|--------|---------------------|
| RT0 | `kAlbedoFormat` | `VK_FORMAT_R8G8B8A8_UNORM` | `albedo.rgb`, **A = 1.0** |
| RT1 | `kNormalRoughnessFormat` | `VK_FORMAT_R16G16B16A16_SFLOAT` | `normal.xyz`, **W = roughness** |
| Depth | `FindDepthFormat()` | depth | standard |

Defined in `Vk_GBufferPass.cpp` L31–32, L113–176. **Do not add a third color RT.**

### 3.3 Material CPU → GPU (unchanged layout)

| Path | C++ type | Shader struct | Fields used in S4 |
|------|----------|---------------|-----------------|
| Batch | `GpuMaterialParams` | `MaterialData` in `GBuffer.frag` | `roughness`, `metallic`, `baseColorFactor` |
| Bindless | `GpuMaterialTableEntry` | `GpuMaterialEntry` in `GBufferFrag_Bindless.frag` | same |

Scene JSON: `Gfx_SceneLoader.cpp` parses `roughness` (default `0.5`), `metallic` (default `0.0`). See [`SceneJSON.md`](SceneJSON.md) § materials.

**Sponza gap:** `Scripts/Generate-SponzaScene.ps1` does **not** emit `roughness`/`metallic` — all materials use defaults. Step 7 fixes this.

### 3.4 Deferred push constants (keep 128 bytes — do not resize)

C++ `Gfx_DeferredLightingPushConstants` in `Gfx/Gfx_ClusterLighting.h` L46–58 — `static_assert` **128 bytes**.

GLSL `DeferredLighting.frag` L21–27 must stay layout-compatible. **S4 strategy:** stop **reading** `specParams.x/y` in shader; leave C++ `BuildPushConstants` writing them (harmless). Only `specParams.z` = debug view stays live.

### 3.5 Environment / lighting defaults

Set in `Vk_SceneHost.cpp` L65–70 on scene load:

```cpp
myFogDistance       = { 0.45f, 32.0f, 1.0f, 0.0f };  // x,y legacy spec; z texture blend; w debug view
myAmbientColor      = { 0.15f, 0.15f, 0.18f, 1.0f };
mySunlightDirection = normalize(-0.35, -0.85, -0.4);
mySunlightColor     = { 0.9f, 0.88f, 0.82f, 1.0f };
```

Sun → GPU: `Vk_ClusterBuildPass.cpp` `WriteSunLightFromEnvironment` → `lights[0]` SSBO → deferred loop.

### 3.6 ImGui

| Panel | File | S4 action |
|-------|------|-----------|
| Lighting | `Util/Util_LightingPanel.cpp` L21–22 | Mark spec/shininess sliders **disabled** or add tooltip "ignored under PBR" — **keep** `fogDistances.z` texture blend slider |
| Render debug | `Util/Util_RenderDebugPanel.cpp` | **Do not break** `myFogDistance.w` debug view encoding |

---

## 4. Non-goals

- IBL / skybox / shadows → **S5**
- SSAO / Hi-Z / post / `FrameGraphBuilder` → **S6–S7**
- Transparent forward PBR shader rewrite
- New shader permutations / `Gfx_ShaderFeature_Pbr` registry entry
- `DescriptorContract_Lit*.json` layout changes (Set 1 material UBO unchanged)
- Golden pixel CI / perf A-B → S7 / epic §D
- `EngineArchitecture.md` edit unless promoting G-buffer packing at task close (optional; see §13)

---

## 5. Target contracts

### 5.1 G-buffer MR packing (S4 lock)

| Target | Content |
|--------|---------|
| RT0 | `rgb` = albedo · `a` = **metallic** ∈ [0,1] |
| RT1 | `xyz` = world normal (unit) · `w` = **roughness** ∈ [0,1] |

Clamp before write: `metallic = clamp(metallic, 0.0, 1.0)`, `roughness = clamp(roughness, 0.0, 1.0)`.

### 5.2 BRDF model (direct sun only)

**Metallic workflow** (Cook-Torrance, one directional light):

- `F0 = mix(vec3(0.04), albedo, metallic)`
- Diffuse: `kD = (1 - F) * (1 - metallic)`; Lambert `kD * albedo / PI`
- Specular: GGX NDF + Schlick F + Smith GGX G
- `Lo = (diffuse + specular) * NdotL * lightColor` per light
- Ambient (until S5): `ambientColor.rgb * albedo` (non-PBR hack; keep for parity with current ambient term)

Shared implementation: new file `VulkanDesktop/Shader/PbrDirect.glsl` (functions only, **no** `#version`).

### 5.3 Parity tolerance (ForwardLit vs HybridDeferred)

| Aspect | Expectation |
|--------|-------------|
| Qualitative | Both show roughness-dependent highlight width; metallic brightens specular |
| Numeric | Small differences OK (forward: per-draw material; deferred: G-buffer decode, FP16 normal RT) |
| Blocker | Hybrid still flat albedo-only OR MR sliders/material JSON have zero effect |

---

## 6. Step 0 — Environment setup

```powershell
$Repo = "<absolute-repo-root>"   # directory containing VulkanDesktop.sln

# Assets (manual §S4 only — CI does not need Sponza)
powershell -File "$Repo\Scripts\Fetch-SponzaMcGuire.ps1"
powershell -File "$Repo\Scripts\Generate-SponzaScene.ps1"

# Toolchain
powershell -File "$Repo\Scripts\Verify-CI.ps1"   # baseline must pass before edits
```

**Shader rebuild (repeat after Steps 1–4):**

```powershell
cmd /c "call `"$Repo\VulkanDesktop\Shader\CompileShader.bat`""
# Commit VulkanDesktop/Shader_Generated/*.spv
```

**Optional but recommended:** add to `VulkanDesktop.vcxproj` `CustomBuild` `AdditionalInputs`: `GBuffer.frag`, `GBufferFrag_Bindless.frag`, `DeferredLighting.frag`, `PbrDirect.glsl` — so MSBuild recompiles when those change.

---

## 7. Step 1 — G-buffer metallic encode

**Files:** `VulkanDesktop/Shader/GBuffer.frag`, `VulkanDesktop/Shader/GBufferFrag_Bindless.frag`

**Change (batch)** — replace L36:

```glsl
// Before:
outAlbedo = vec4(albedo, 1.0);

// After:
const float metallic = clamp(material.metallic, 0.0, 1.0);
const float roughness = clamp(material.roughness, 0.0, 1.0);
outAlbedo = vec4(albedo, metallic);
outNormalRoughness = vec4(N, roughness);
```

**Change (bindless)** — replace L46–47 similarly using `mat.metallic` / `mat.roughness`.

**Verify:** compile shaders; G0. No C++ changes.

**Progress checkpoint:** files touched; `Verify-CI` exit code.

---

## 8. Step 2 — `PbrDirect.glsl` + compile script

### 8.1 Create `VulkanDesktop/Shader/PbrDirect.glsl`

Provide (at minimum):

```glsl
// PbrDirect.glsl — included by lit + deferred fragments. No #version here.
const float PBR_PI = 3.14159265358979323846;

float Pbr_DistributionGGX(vec3 N, vec3 H, float roughness) { /* standard GGX */ }
float Pbr_GeometrySchlickGGX(float NdotV, float roughness) { /* k = (r+1)^2/8 */ }
float Pbr_GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) { /* GGX Smith */ }
vec3  Pbr_FresnelSchlick(float cosTheta, vec3 F0) { /* Schlick */ }

// Returns RGB contribution from one directional light (not multiplied by NdotL yet if you prefer outer multiply).
vec3 Pbr_EvalDirect(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 radiance)
{
    // Implement metallic workflow; roughness = perceptual [0,1].
    // radiance = lightColor (RGB intensity).
}
```

Use `max(..., 0.0001)` guards on denominators.

### 8.2 Update `CompileShader_Glslc.bat`

Add include path to **every** `glslc` invocation that will `#include "PbrDirect.glsl"`:

```bat
REM No quotes around %SHADER_DIR% — trailing backslash breaks -I"%SHADER_DIR%" in cmd.
"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%DeferredLighting.frag" -o "%GEN_DIR%DeferredLightingFrag.spv"
"%GLSLC%" -I%SHADER_DIR% "%SHADER_DIR%TriangleFrag_Lit.frag" -o "%PS_OUT%"
REM ... same for TriangleFrag_Lit_Bindless, ALPHA_CLIP variant; GBuffer does not include PbrDirect
```

Run full `CompileShader.bat`; commit all affected `.spv`.

**Verify:** G0.

---

## 9. Step 3 — `DeferredLighting.frag` Cook-Torrance

**File:** `VulkanDesktop/Shader/DeferredLighting.frag`

1. After `#version 450`, add: `#include "PbrDirect.glsl"`

2. In `main()`, replace L63–90 body:

```glsl
const vec4 albedoMetallic = texture(gbufferAlbedo, vUV);
const vec3 albedo = albedoMetallic.rgb;
const float metallic = albedoMetallic.a;
const vec4 normalRoughness = texture(gbufferNormalRoughness, vUV);
const vec3 N = normalize(normalRoughness.xyz);
const float roughness = normalRoughness.w;
// ... depth / worldPos / clusterId unchanged ...

vec3 lit = pc.ambientColor.rgb * albedo;
const vec3 V = normalize(pc.viewWorldPos.xyz - worldPos);

for (uint i = 0u; i < lightCount; ++i) {
    const ClusterLight light = lights[cluster.lightIndices[i]];
    const vec3 L = normalize(light.direction.xyz);
    const float NdotL = max(dot(N, L), 0.0);
    if (NdotL > 0.0) {
        lit += Pbr_EvalDirect(N, V, L, albedo, metallic, roughness, light.color.rgb) * NdotL;
    }
}
```

3. **Remove** Blinn-Phong use of `pc.specParams.x/y`. Keep `applyDebugView` using `pc.specParams.z`.

4. **C++:** `Vk_DeferredLightingPass.cpp` `BuildPushConstants` — optional comment that `specularStrength`/`shininess` are legacy; **no struct size change**.

5. Update file header comment: Cook-Torrance direct PBR v1.

**Verify:** G0 + G0-smoke.

---

## 10. Step 4 — Forward `TriangleFrag_Lit*` parity

**Files:** `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag`

1. `#include "PbrDirect.glsl"` after `#version`.

2. Replace Blinn-Phong block (ambient + diffuse + spec) with:

```glsl
const float metallic = clamp(material.metallic, 0.0, 1.0);  // bindless: mat.metallic
const float roughness = clamp(material.roughness, 0.0, 1.0);
const vec3 V = normalize(envData.viewWorldPos.xyz - inWorldPos);
const vec3 L = normalize(envData.sunlightDirection.xyz);
const float NdotL = max(dot(N, L), 0.0);

vec3 color = envData.ambientColor.rgb * albedo;
if (NdotL > 0.0) {
    color += Pbr_EvalDirect(N, V, L, albedo, metallic, roughness, envData.sunlightColor.rgb) * NdotL;
}
outColor = vec4(color, clamp(material.alpha, 0.0, 1.0));
```

3. Preserve alpha mask / `ALPHA_CLIP` / `applyDebugView` blocks unchanged.

4. Re-run `ReflectShaders_Lit.bat` path via full compile (descriptor contracts unchanged if Set 0/1 bindings unchanged).

**Verify:** G0 + G0-smoke.

---

## 11. Step 5 — UI + docs cleanup

| File | Edit |
|------|------|
| `Util/Util_LightingPanel.cpp` | `ImGui::BeginDisabled()` around spec strength + shininess sliders, tooltip "No effect under PBR (S4)" |
| `RenderCore/Vk_Types.h` | Comment on `myFogDistance.x/y`: legacy, unused by PBR shaders |
| `SceneJSON.md` L153–154 | Change description: roughness/metallic **consumed** by lit + G-buffer shaders |
| `forward-stage1.md` §3 gap table | Mark PBR BRDF row as S4 in progress / done at closeout |

**Verify:** G0 (no GfxTests change expected).

---

## 12. Step 6 — Sponza material MR seeding (required for G5)

Without varied MR, acceptance visuals fail (all materials = 0.5 / 0.0).

**File:** `Scripts/Generate-SponzaScene.ps1` — in material build loop (~L250), after `$matEntry` base fields, add heuristics from `$matName` / `$meshId`:

| Name contains (case-insensitive) | `roughness` | `metallic` |
|----------------------------------|-------------|------------|
| `fabric`, `curtain`, `carpet` | `0.88` | `0.0` |
| `chain`, `vase_round`, `flagpole` | `0.35` | `0.85` |
| `floor` | `0.55` | `0.0` |
| `brick`, `arch`, `column` | `0.72` | `0.0` |
| default | `0.65` | `0.0` |

Emit JSON fields `"roughness": <f>, "metallic": <f>` on each material entry.

**Regenerate scene:**

```powershell
powershell -File Scripts/Generate-SponzaScene.ps1
```

**Verify:** open `Data/Scenes/sponza.json` — multiple distinct `roughness` values exist.

---

## 13. Step 7 — Validation + parity notes

### 7.1 CI (required)

```powershell
powershell -File Scripts/Verify-CI.ps1      # exit 0
powershell -File Scripts/Verify-Smoke.ps1   # exit 0
```

### 7.2 Manual Sponza (required for task close)

```powershell
$Repo = "<repo-root>"
& "$Repo\x64\Debug\VulkanDesktop.exe" `
  --asset-root $Repo `
  --config "$Repo\Config\engine.json" `
  --no-validation --smoke-seconds 6
```

**Log assertions** (tail `Logs/engine_runtime_log.txt`):

- `[SCENE] Parsed scene v1 name='sponza'` (or equivalent load line)
- `[FG] HybridDeferred: GBufferOpaque -> ClusterBuild -> DeferredLighting -> ForwardTransparent`
- `[APP] Engine exited run loop normally` · exit code **0**

### 7.3 Visual acceptance (required)

On HybridDeferred + Sponza:

- [ ] Fabric/curtain: broad soft highlights (high roughness)
- [ ] Chain/metal mesh: sharper, brighter specular
- [ ] Changing material roughness in JSON (one test material) visibly changes highlight size after reload

### 7.4 Preset parity (required notes in Progress)

```powershell
# Forward baseline — restart between presets (no hot-reload)
& "$Repo\x64\Debug\VulkanDesktop.exe" --asset-root $Repo --render-preset ForwardLit `
  --scene Data/Scenes/sponza.json --no-validation --smoke-seconds 4
```

Record in Progress: qualitative match / known differences (see §5.3).

### 7.5 Optional debug

ImGui → Render Debug → World normal / Depth still work. MR debug modes **not required** for S4.

---

## 14. Progress logging

Append to [`s4-pbr-gbuffer_Progress.md`](s4-pbr-gbuffer_Progress.md) after each step:

```markdown
## YYYY-MM-DD — Step N

- **Files:** …
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0 (or N/A + reason)
- **Notes:** …
```

**Closeout template (task end):**

```markdown
## Closeout — YYYY-MM-DD

- **Outcome:** …
- **Verification:** commands + exit 0; log lines: …
- **Deviations:** none | …
```

---

## 15. Task close checklist (vibe coding)

1. All steps 1–7 done; Closeout in Progress (≤30 lines)
2. `s4-pbr-gbuffer_Plan.md` → `Status: Closed (YYYY-MM-DD)`
3. Move Plan + Progress → `Docs/Archived/plans/`
4. Add line to `Docs/Archived-Plan.md`; remove S4 row from `Active-Plan.md` queue (or mark done per repo convention)
5. Clear `Docs/README.md` **WIP task** pointer
6. `EngineArchitecture.md` §7 — **only if** promoting G-buffer MR table to locked policy

---

## 16. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `Assert-ShaderDrift` fails | `.spv` not committed | Run `CompileShader.bat`, `git add Shader_Generated` |
| glslc cannot find `PbrDirect.glsl` | Missing `-I` | Fix `CompileShader_Glslc.bat` §8.2 |
| Black/deferred pass empty | G-buffer barrier | Unchanged in S4; check prior S3 regressions |
| No visual MR difference on Sponza | All materials default 0.5 | Run Step 6 regenerate |
| Validation error push constant size | Changed `Gfx_DeferredLightingPushConstants` size | Revert; keep 128 bytes |
| `ForwardLit` unchanged | Wrong preset | Use `--render-preset ForwardLit` + restart |
| Bindless hybrid flat MR | Only batch `GBuffer.frag` updated | Also edit `GBufferFrag_Bindless.frag` |

---

## 17. File checklist (complete touch list)

| Action | Path |
|--------|------|
| Edit | `Shader/GBuffer.frag` |
| Edit | `Shader/GBufferFrag_Bindless.frag` |
| **Create** | `Shader/PbrDirect.glsl` |
| Edit | `Shader/DeferredLighting.frag` |
| Edit | `Shader/TriangleFrag_Lit.frag` |
| Edit | `Shader/TriangleFrag_Lit_Bindless.frag` |
| Edit | `Shader/CompileShader_Glslc.bat` |
| Edit | `Util/Util_LightingPanel.cpp` |
| Edit | `Scripts/Generate-SponzaScene.ps1` |
| Comment | `RenderCore/Vk_Types.h`, `RenderCore/Vk_DeferredLightingPass.cpp` |
| Docs | `SceneJSON.md`, `forward-stage1.md` (gap row) |
| Commit | `Shader_Generated/GBufferFrag.spv`, `GBufferFrag_Bindless.spv`, `DeferredLightingFrag.spv`, `TrianglePix*.spv` |
| Optional | `VulkanDesktop.vcxproj` AdditionalInputs |

**Do not edit:** `Vk_GBufferPass.cpp` (formats), `ClusterBuild.comp`, `GpuMaterialParams` layout, `DescriptorContract_*.json`.

---

## 18. References

- S3 gaps: [`Archived/S3-回顾总结.md`](Archived/S3-回顾总结.md)
- S3 G-buffer v0: [`Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md`](Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md)
- Bindless maint: [`Archived/plans/shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance
- Descriptor policy: [`EngineArchitecture.md`](EngineArchitecture.md) §6
- Build/smoke: [`CLI.md`](CLI.md) · `.cursor/rules/vulkan-smoke-test.mdc`
