# Plan: s5-ibl-shadows — IBL, skybox, directional shadows (Lighting-2)

**Status:** Closed (2026-06-12)  
**Progress:** [`Archived/plans/s5-ibl-shadows_Progress.md`](Archived/plans/s5-ibl-shadows_Progress.md)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) queue #1 · [`Wishlist.md`](Wishlist.md) § S5  
**Epic:** [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §C (IBL + environment)  
**Depends on:** S4 PBR + G-buffer ✓ (2026-06-12)  
**Validation runbook:** [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) § S5

---

## 0. Agent quick start (zero prior context)

You are implementing **S5** in repo `SiriusEngine_VulkanReboot` (VulkanDesktop, Windows, MSBuild Debug|x64).

**Read first (10 min):** this plan §1–§5, then open every file in §3 **Current state** and §17 **Touch list**.

**Hard constraints (do not violate without plan amendment):**

| Constraint | Source |
|------------|--------|
| G-buffer RT formats unchanged (RT0 albedo+metallic, RT1 normal+roughness) | S4 lock · `Vk_GBufferPass.cpp` |
| `Gfx_DeferredLightingPushConstants` stays **128 bytes** | `Gfx_ClusterLighting.h` static_assert |
| **No new shader permutations** (M7 freeze: registry stays `lit` + `lit_alpha_clip` only) | `Gfx_ShaderPermutation.h` · bindless policy §Maintenance |
| Shadow / IBL on/off = **runtime toggles** (UBO / push fields), not `#ifdef SHADOWS` permutations | This plan §5.4 |
| CI G0 must pass **without network** — ship minimal env assets in-repo or default toggles off for `stress.json` | §6 Step 0 |
| After every shader edit: `VulkanDesktop\Shader\CompileShader.bat` → commit `Shader_Generated/*.spv` | `Assert-ShaderDrift.ps1` |

**Verify after each step:** `powershell -File Scripts/Verify-CI.ps1` (minimum). GPU/runtime steps also run `Verify-Smoke.ps1`.

**Log progress:** append checkpoint to [`s5-ibl-shadows_Progress.md`](s5-ibl-shadows_Progress.md) (template §14).

---

## 1. Problem

S4 closed **Cook-Torrance direct sun + flat ambient hack** on the hybrid chain. Wishlist §S5 and epic §C require **environment lighting + visible sky + directional shadows** before SSAO/post (S6–S7) and G4 acceptance.

| Gap | Symptom today | Key locations |
|-----|---------------|---------------|
| No sky / env background | Openings in Sponza show cleared black/flat resolve color | `Vk_GBufferPass.cpp` hybrid resolve color clear `{0,0,0,1}` |
| No IBL | Interiors flat; `ambientColor * albedo` non-PBR hack | `DeferredLighting.frag` L75 · `PbrDirect.glsl` `Pbr_LitWithSunAndAmbient` |
| No shadow map | Sun casts no contact shadows on ground/arch | No `Vk_*Shadow*` module |
| Forward/deferred env mismatch risk | Forward uses same ambient hack | `TriangleFrag_Lit*.frag` |
| No toggles | Cannot A/B shadows or IBL | `Util_LightingPanel.cpp` · `engine.json` |
| Cubemap loader missing | `UtilLoader::LoadTexture` is **2D only** (`stb_image`, `VK_IMAGE_VIEW_TYPE_2D`) | `Util_Loader.cpp` |

**User-visible target (Sponza, HybridDeferred):** sun shadow on floor/columns; sky gradient at roof openings; IBL fills interior bounce without blowing exposure.

---

## 2. Goals (definition of done)

| ID | Deliverable | Objective test |
|----|-------------|----------------|
| G1 | Environment assets | In-repo default env (irradiance + prefilter + BRDF LUT + sky cubemap); optional fetch script for HDRI source |
| G2 | IBL split-sum | Diffuse irradiance + specular prefiltered cubemap + 2D BRDF LUT; shared `PbrIbl.glsl` |
| G3 | Deferred integration | `DeferredLighting` replaces ambient hack; PCF shadow on sun; sky at far depth |
| G4 | Forward integration | `TriangleFrag_Lit*` same sun shadow + IBL inputs as deferred |
| G5 | Shadow pass v0 | Single 2048² depth map; stable ortho frustum from sun; recorded before G-buffer |
| G6 | Toggles | `engine.json` + ImGui: shadows on/off, IBL on/off, IBL intensity |
| G7 | CI | `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0 (stress preset — toggles may default off) |
| G8 | Sponza manual | §13 validation checklist + PR screenshots |

---

## 3. Current state snapshot

### 3.1 Frame graph (HybridDeferred — default `Config/engine.json`)

```
GBufferOpaque → ClusterBuild → [barrier] → [copy G-buffer depth → swapchain depth]
  → HybridResolveRP: DeferredLighting → ForwardTransparent → present
```

- Orchestration: `VulkanDesktop/RenderCore/Vk_GBufferPass.cpp` (`RecordFrame`, L403+)
- Preset gate: `Gfx_RenderPreset::IsHybridDeferred` in `Vk_ScenePasses.cpp`
- Default scene: `Config/engine.json` → `"scene": "Data/Scenes/sponza.json"`, `"renderPreset": "HybridDeferred"`
- First-frame log: `[FG] HybridDeferred: GBufferOpaque -> ClusterBuild -> DeferredLighting -> ForwardTransparent`

**S5 target graph (FG v0 manual chain — not FrameGraphBuilder yet):**

```
ShadowMapDirectional → GBufferOpaque → ClusterBuild → [barrier + depth copy]
  → HybridResolveRP: DeferredLighting (+ IBL + shadow + sky composite) → ForwardTransparent (+ IBL + shadow) → present
```

Update the one-shot log string when Step 8 lands.

### 3.2 G-buffer (unchanged — do not edit formats)

| RT | Format | Packing (S4 lock) |
|----|--------|-------------------|
| RT0 | `VK_FORMAT_R8G8B8A8_UNORM` | rgb=albedo, **a=metallic** |
| RT1 | `VK_FORMAT_R16G16B16A16_SFLOAT` | xyz=normal, **w=roughness** |
| Depth | platform depth | standard |

### 3.3 Lighting data paths today

| Data | CPU | GPU path |
|------|-----|----------|
| Sun direction/color | `Vk_SceneHost.cpp` L68–69 defaults | `WriteSunLightFromEnvironment` → `lights[0]` SSBO |
| Ambient (IBL placeholder) | `myAmbientColor` L65 | Deferred push `ambientColor` · forward `envData.ambientColor` |
| Camera / view pos | `PrepareFrameCpu` | `GpuEnvironmentData.myViewWorldPos` · deferred push `viewWorldPos` |
| Debug view | `Util_RenderDebugPanel` | `myFogDistance.w` |

**Sun direction convention (locked):** `mySunlightDirection.xyz` = direction **from surface toward sun** (normalized each frame in ImGui panel). Matches `ClusterBuild` / `Pbr_EvalDirect`.

### 3.4 Descriptor layouts today

**Forward / G-buffer scene pipelines — Set 0 (frame):**

| Binding | Name | Type | Stages |
|---------|------|------|--------|
| 0 | CameraData | UBO | VERTEX |
| 1 | EnvironmentData | UBO | FRAGMENT |

Source of truth: `Vk_Enum.h` · `DescriptorContract_LitBatch.json`.

**Deferred lighting — separate Set 0 (5 bindings):**

| Binding | Content |
|---------|---------|
| 0–1 | G-buffer albedo, normal+roughness samplers |
| 2–3 | Cluster lights SSBO, cluster lists SSBO |
| 4 | G-buffer depth sampler |

Source: `Vk_DeferredLightingPass.cpp` L102–107, `DeferredLighting.frag` L28–36.

### 3.5 Shader shared library (S4)

| File | Role |
|------|------|
| `Shader/PbrDirect.glsl` | Cook-Torrance direct; `Pbr_EvalDirect`, `Pbr_LitWithSunAndAmbient` |
| `Shader/DeferredLighting.frag` | Fullscreen deferred resolve |
| `Shader/TriangleFrag_Lit*.frag` | Forward lit (batch + bindless) |

`Pbr_LitWithSunAndAmbient` comment: *"Forward path: diffuse-only ambient (until S5 IBL)"* — **replace in Step 5**.

### 3.6 Permutation / preset policy

- `PermutationRegistry.json`: only `lit`, `lit_alpha_clip` (ALPHA_CLIP feature).
- `Gfx_ShaderFeature_Shadows` / `Gfx_ShaderFeature_Ibl` exist in enum but **M7-frozen** — do **not** add registry entries in S5.
- Render presets: `ForwardLit`, `HybridDeferred` (`Gfx_RenderPreset`) — both must compile; shadow/IBL toggles apply to **both** when lit shaders run.

### 3.7 ImGui / config

| Panel | File | S5 action |
|-------|------|-----------|
| Lighting | `Util/Util_LightingPanel.cpp` | Add shadow/IBL toggles + IBL intensity; keep sun/ambient sliders (ambient becomes IBL scale fallback) |
| Render debug | `Util/Util_RenderDebugPanel.cpp` | Optional: shadow map debug view mode (backlog OK if timeboxed) |
| Config | `Util/Util_EngineConfig.cpp` | Parse `lighting.*` JSON + CLI overrides |

---

## 4. Non-goals

- Multi-cascade / CSMC / shadow atlasing → backlog (S5 = **single cascade v0**)
- Point / spot shadow maps → backlog
- Real-time env map capture / dynamic sky → backlog (use **static baked** assets)
- SSAO, Hi-Z, tonemap, bloom, `FrameGraphBuilder` → **S6–S7**
- Lifting M7 permutation freeze / adding `SHADOWS`/`IBL` glslc variants → **not S5**
- DDGI → **S8** (after G4)
- Golden-pixel CI / perf A-B thresholds → S7 / epic §D
- `EngineArchitecture.md` edit at kickoff — only at **task close** if Set 0 lighting bindings promoted (§15)

---

## 5. Target contracts

### 5.1 IBL model (split-sum approximation)

**Static assets (offline baked, loaded at init):**

| Asset | Typical size | Binding role |
|-------|--------------|--------------|
| `irradiance.cubemap` | 32×32×6 RGB16F or RGBA8 | Diffuse convolution of env |
| `prefiltered_env.cubemap` | 512×512×6 + mips RGB16F | Specular prefilter per roughness |
| `brdf_lut.png` | 512×512 RG16F stored as RGBA8 | Split-sum `(A, B)` lookup |
| `sky_env.cubemap` | 512×512×6 (may alias prefilter mip0 or separate) | Skybox sample |

**Shading (metallic workflow, matches S4 `PbrDirect`):**

```glsl
// PbrIbl.glsl (new) — included by deferred + forward
vec3 F0 = mix(vec3(0.04), albedo, metallic);
vec3 diffuseIBL = irradiance * albedo;  // or kD * irradiance * albedo with kS from env
vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);  // split-sum
vec3 ambient = (diffuseIBL * (1.0 - metallic) + specularIBL) * iblIntensity;
```

Replace `ambientColor * albedo` with `ambient` when `iblEnabled`; when off, keep S4 flat ambient for parity debugging.

**Offline bake (v0):** document one acceptable path in Step 0 — e.g. Python/`cmgen` script **or** ship only pre-baked files under `Data/Environments/default/` committed to repo. Agents must not block implementation on HDRI download.

### 5.2 Directional shadow v0

| Parameter | v0 value |
|-----------|----------|
| Map size | 2048 × 2048 |
| Format | `VK_FORMAT_D32_SFLOAT` depth-only |
| Cascade count | **1** |
| Light type | Directional from `mySunlightDirection` |
| Frustum | Orthographic; **stable** — fit to camera frustum corners transformed to light space; snap texel grid |
| Bias | Constant + slope-scaled (tunable in ImGui) |
| Filter | **PCF 3×3** (9 taps) in shader |
| Casters | Opaque scene draws (same instance/indirect path as G-buffer) |

**Shadow matrix contract (`GpuLightingGlobals` UBO):**

```cpp
struct GpuLightingGlobals {
    glm::mat4 myLightViewProj;      // world → shadow clip
    glm::vec4 myShadowParams;       // x=bias, y=normalOffset, z=enabled (0/1), w=mapInvSize
    glm::vec4 myIblParams;          // x=iblIntensity, y=iblEnabled (0/1), z/w pad
};
// std140, ~96 bytes — must match GLSL LightingGlobals block
```

### 5.3 Sky composite (deferred)

For pixels where reconstructed G-buffer depth ≥ **0.999** (far plane):

```glsl
lit = textureLod(skyCubemap, normalize(worldPos - cameraPos), 0.0).rgb;
```

Skip direct/IBL/shadow for sky pixels. Forward transparent unchanged (does not write sky).

**Alternative (acceptable if simpler):** separate fullscreen sky draw after deferred with `depthTest=LESS_EQUAL`, `depthWrite=false`, clearing depth to 1.0 before draw — document choice in Progress.

### 5.4 Runtime toggles (not permutations)

| Toggle | Storage | Shader behavior |
|--------|---------|-----------------|
| `shadowsEnabled` | `GpuLightingGlobals.myShadowParams.z` + config | Skip shadow map pass record when false; shader uses `1.0` visibility |
| `iblEnabled` | `GpuLightingGlobals.myIblParams.y` + config | Fall back to S4 `ambientColor * albedo` |
| `iblIntensity` | `GpuLightingGlobals.myIblParams.x` | Scales IBL contribution |

Config schema (add to `engine.json`):

```json
"lighting": {
  "shadowsEnabled": true,
  "iblEnabled": true,
  "iblIntensity": 1.0,
  "environment": "Data/Environments/default"
}
```

**Stress / CI:** `Config/engine.stress.json` should set `"shadowsEnabled": false, "iblEnabled": false` unless minimal assets guaranteed — keeps G0-smoke fast and offline.

### 5.5 Descriptor extension (Set 0 frame + deferred set)

**Forward / G-buffer scene Set 0 — add bindings 2–6** (policy change → Architecture §6 at close):

| Binding | Name | Type |
|---------|------|------|
| 2 | LightingGlobals | UBO |
| 3 | ShadowMap | `sampler2DShadow` or depth compare sampler |
| 4 | IrradianceMap | cubemap sampler |
| 5 | PrefilterMap | cubemap sampler |
| 6 | BrdfLut | 2D sampler |

**Deferred Set 0 — add bindings 5–9** (same resources; duplicate layout for deferred pass):

| Binding | Same as |
|---------|---------|
| 5 | LightingGlobals UBO |
| 6 | ShadowMap |
| 7 | IrradianceMap |
| 8 | PrefilterMap |
| 9 | BrdfLut |

Update `Vk_Enum.h`, `DescriptorContract_LitBatch.json`, `DescriptorContract_LitBindless.json`, `Vk_DescriptorSystem.cpp`, `Vk_DeferredLightingPass.cpp`, reflection scripts.

**Bindless note:** bindless forward frag uses Set 1 only for materials; **Set 0 frame bindings still apply** — extend bindless contract JSON accordingly.

### 5.6 Direct light + shadow composition

In deferred sun loop (after S4 structure):

```glsl
vec3 radiance = light.color.rgb;
if (shadowsEnabled) {
    float vis = Pbr_ShadowPCF(shadowMap, lightViewProj, worldPos, normal, shadowParams);
    radiance *= vis;
}
lit += Pbr_EvalDirect(N, V, L, albedo, metallic, roughness, radiance);
// NdotL already inside Pbr_EvalDirect — do not double-multiply
```

Forward: same shadow function using `inWorldPos` / `inWorldNormal`.

---

## 6. Step 0 — Environment setup & assets

```powershell
$Repo = "<absolute-repo-root>"

# Sponza (manual validation only — CI does not require)
powershell -File "$Repo\Scripts\Fetch-SponzaMcGuire.ps1"
powershell -File "$Repo\Scripts\Generate-SponzaScene.ps1"

# Baseline must pass before edits
powershell -File "$Repo\Scripts\Verify-CI.ps1"
```

### 6.1 Ship in-repo default environment

Create `Data/Environments/default/` with **committed** files (small enough for git):

```
manifest.json          # lists cubemap faces / LUT paths
irradiance/            # 6 faces OR KTX placeholder — see 6.2
prefilter/             # 6 faces per mip prefix OR single DDS/KTX
brdf_lut.png           # 512×512
sky/                   # optional separate sky (may reuse prefilter mip0)
```

**`manifest.json` schema (v1):**

```json
{
  "schemaVersion": 1,
  "name": "default",
  "irradiance": "irradiance",
  "prefilter": "prefilter",
  "brdfLut": "brdf_lut.png",
  "sky": "sky"
}
```

### 6.2 Asset authoring options (pick one, document in Progress)

| Option | Pros | Cons |
|--------|------|------|
| **A — Pre-baked only** | CI offline; zero tool deps | Large binary in git |
| **B — `Scripts/Fetch-S5Environment.ps1`** | Smaller repo | Network on dev machines |
| **C — `Scripts/Bake-IblFromHdr.ps1`** | Reproducible | Needs external tool (cmgen / custom) |

**Minimum for implementation:** Option A with **low-res** cubemaces (32 irradiance, 128 prefilter) — visually weak but proves pipeline; upgrade resolution before closeout screenshots.

### 6.3 Optional fetch script

Add `Scripts/Fetch-S5Environment.ps1` (pattern: `Fetch-PolyHavenStressTextures.ps1`) downloading one CC0 HDRI + instructions to run bake script. **Not invoked by CI.**

**Verify Step 0:** manifest paths resolve under `--asset-root`; G0 still exit 0.

---

## 7. Step 1 — `PbrIbl.glsl` + shadow helpers

**Create:** `VulkanDesktop/Shader/PbrIbl.glsl`

Required functions:

```glsl
// Requires samplers bound by caller — no #version
vec2  Pbr_BrdfLut(float NdotV, float roughness, sampler2D brdfLut);
vec3  Pbr_SampleIrradiance(samplerCube irradiance, vec3 N);
vec3  Pbr_SamplePrefilter(samplerCube prefilter, vec3 R, float roughness);
float Pbr_ShadowPCF(sampler2DShadow shadowMap, mat4 lightViewProj,
                    vec3 worldPos, vec3 N, vec4 shadowParams);
vec3  Pbr_EvalIbl(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
                  /* samplers + intensity + enabled flag */);
```

**Update:** `CompileShader_Glslc.bat` — add `-I%SHADER_DIR%` for any new compiles that `#include` PbrIbl (deferred + forward frags).

**Verify:** compile only; G0.

---

## 8. Step 2 — Cubemap + LUT loader (`Vk_IblResources`)

**Create:**

| File | Role |
|------|------|
| `RenderCore/Vk_IblResources.h` | State: images, views, samplers, manifest path |
| `RenderCore/Vk_IblResources.cpp` | Load manifest, upload cubemaces + LUT |

**Extend:** `Util/Util_Loader.h/.cpp`

- `LoadCubemapFaces(...)` — 6 PNG paths → `VkImage` array layer cubemap `VK_IMAGE_VIEW_TYPE_CUBE`
- `LoadImage2D(...)` — refactor from `LoadTexture` core or share staging path for BRDF LUT

**Wire:**

- `Vk_Core::Init` — after device ready, load env from `Util_EngineConfig::GetEnvironmentPath()` (default `Data/Environments/default`)
- `Vk_FrameUniformUploader` or new `Vk_LightingUniformUploader` — fill `GpuLightingGlobals` each frame (shadow matrix + toggles)
- Log once: `[IBL] Loaded environment 'default' irradiance=32 prefilter=128`

**Failure policy:** if manifest missing and `assetVerify=strict`, fail load; if `iblEnabled=false`, skip bind (null descriptor or dummy 1×1 cubemap — prefer **dummy** to keep layout valid).

**Verify:** G0 + G0-smoke with stress toggles off.

---

## 9. Step 3 — Directional shadow map pass

**Create:**

| File | Role |
|------|------|
| `RenderCore/Vk_ShadowMapPass.h` | RT, render pass, pipeline, light matrix cache |
| `RenderCore/Vk_ShadowMapPass.cpp` | Init/Destroy/Recreate/Record |
| `Shader/ShadowMap.vert` | Reuse scene transforms (same as G-buffer VS contract) |
| `Shader/ShadowMap.frag` | Empty or depth-only (`// depth only`) |
| `Shader/ShadowMapDepth.vert` | *Alternative:* duplicate TriangleVertex with light MVP push |

**C++ shadow matrix:**

- Compute camera frustum 8 corners in world space
- Transform to light view space; build ortho `[min,max]` with padding
- **Snap** ortho extents to `(2*extent)/mapSize` grid to reduce shimmer
- Store `myLightViewProj = lightProj * lightView`

**Record placement:** `Vk_GBufferPass::RecordFrame` — **first** call:

```cpp
if (lighting.shadowsEnabled) {
    Vk_ShadowMapPass::Record(aCore, cmd, packet, ...);
}
```

**Draw path:** reuse `Vk_ScenePasses::RecordOpaquePacketDraws` with shadow pipeline + light view/proj push/UBO override.

**Barriers:** shadow depth → `SHADER_READ_ONLY` for deferred/forward sample.

**Verify:** G0-smoke; RenderDoc shows ShadowMap pass before GBuffer (manual note in Progress).

---

## 10. Step 4 — Descriptor + uniform wiring

**Edit (same change set):**

1. `Vk_Enum.h` — extend `eVk_FrameBindingCount` + new binding enums
2. `DescriptorContract_LitBatch.json` + bindless JSON — bindings 2–6
3. `Vk_DescriptorSystem.cpp` — allocate/update frame set with IBL + shadow views
4. `Gfx/Gfx_LightingGlobals.h` — **new** CPU struct mirroring UBO
5. `Vk_DeferredLightingPass.cpp` — extend layout to 10 bindings; update pool sizes
6. `Vk_Initializer.cpp` / frame descriptor update path — write lighting globals each frame
7. Run `ReflectShaders_Lit.bat` after GLSL struct adds (if forward EnvironmentData unchanged, reflect may still run for validation)

**Push constants:** keep 128 bytes — store **only** `invViewProj`, grid, debug view; move shadow/IBL toggles to `GpuLightingGlobals` UBO.

**Repurpose** deferred push `legacySpecularStrength` / `legacyShininess` fields only if needed for debug; prefer UBO for toggles.

**Verify:** G0; validation layers clean on startup.

---

## 11. Step 5 — `DeferredLighting.frag` (IBL + shadow + sky)

**File:** `VulkanDesktop/Shader/DeferredLighting.frag`

1. `#include "PbrIbl.glsl"`
2. Add bindings 5–9 matching §5.5
3. Replace L75 `lit = pc.ambientColor.rgb * albedo` with `Pbr_EvalIbl(...)` when enabled; else S4 ambient
4. In sun loop: multiply radiance by `Pbr_ShadowPCF` when enabled
5. Before output: if `depth >= 0.999`, replace `lit` with sky cubemap sample
6. Keep `applyDebugView` behavior

**C++:** `BuildPushConstants` unchanged size; upload lighting UBO before draw.

**Verify:** G0-smoke off; manual Sponza with toggles on — interior not blown out (adjust `iblIntensity` default ~0.8–1.2 in Progress).

---

## 12. Step 6 — Forward `TriangleFrag_Lit*` parity

**Files:** `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag`

1. `#include "PbrIbl.glsl"`
2. Add Set 0 bindings 2–6 (LightingGlobals, shadow, IBL samplers) — match batch contract
3. Replace `Pbr_LitWithSunAndAmbient` call with:
   - IBL ambient term (same as deferred)
   - Direct sun with shadow PCF
4. Update `PbrDirect.glsl` — change `Pbr_LitWithSunAndAmbient` to delegate to IBL path or mark deprecated

**Verify:** G0 + G0-smoke; transparent objects on Sponza receive consistent sun shadow (qualitative).

---

## 13. Step 7 — Config + ImGui toggles

**Config:**

- `Util_EngineConfig.h/.cpp` — parse `lighting` object; CLI `--shadows` / `--no-shadows` / `--ibl` / `--ibl-intensity` (optional but recommended — mirror existing CLI style in `Util_EngineConfig.cpp`)
- `Config/engine.json` — add lighting block (defaults on for dev)
- `Config/engine.stress.json` — shadows/IBL **off**
- `Config/engine.benchmark.json` — on for perf captures

**ImGui:** `Util_LightingPanel.cpp`

- Checkbox Shadows enabled
- Checkbox IBL enabled
- Slider IBL intensity `[0, 3]`
- Tooltip: ambient color scales legacy fallback when IBL off

**Log on change (once):** `[LIGHTING] shadows=1 ibl=1 iblIntensity=1.0`

**Verify:** toggle shadows → contact shadow appears/disappears on Sponza floor; toggle IBL → interior brightness changes.

---

## 14. Step 8 — Frame graph log + integration polish

1. Update `[FG] HybridDeferred: ...` log in `Vk_GBufferPass.cpp` to include `ShadowMapDirectional` prefix when shadows enabled
2. Ensure resize recreates shadow + IBL resources (`RecreateForExtent` hooks in `Vk_Core` swapchain rebuild path — mirror `Vk_DeferredLightingPass::RecreateForExtent`)
3. Command buffer debug labels: `Pass=ShadowMap`, existing deferred/transparent labels unchanged
4. Comment headers in new modules per `cpp-comments.mdc`

**Verify:** full §13 manual checklist + G0 + G0-smoke.

---

## 15. Verification

### 15.1 CI (required every step / closeout)

```powershell
powershell -File Scripts/Verify-CI.ps1
powershell -File Scripts/Verify-Smoke.ps1
```

### 15.2 Manual Sponza (required for task close)

```powershell
$Repo = "<repo-root>"
& "$Repo\x64\Debug\VulkanDesktop.exe" `
  --asset-root $Repo `
  --config "$Repo\Config\engine.json" `
  --no-validation --smoke-seconds 6
```

**Visual (HybridDeferred):**

- [ ] Sky visible at roof openings (not black)
- [ ] Sun shadow on ground + column bases
- [ ] IBL fills interior; specular on metal still visible; exposure not clipped
- [ ] ImGui: shadows off → shadows disappear; IBL off → flat ambient returns

**ForwardLit parity note (required in Progress):**

```powershell
& "$Repo\x64\Debug\VulkanDesktop.exe" --asset-root $Repo --render-preset ForwardLit `
  --scene Data/Scenes/sponza.json --no-validation --smoke-seconds 4
```

Qualitative: forward shows same sun direction + shadow/IBL toggles where applicable.

**Log assertions:**

- `[SCENE] Parsed scene v1 name='sponza'`
- `[FG] HybridDeferred:` chain includes shadow when enabled
- `[IBL] Loaded environment`
- Exit 0 · `[APP] Engine exited run loop normally`

### 15.3 RenderDoc (recommended)

Capture one frame; confirm draw order: **ShadowMap → GBufferOpaque → DeferredLighting**; deferred PS samples shadow + irradiance bindings.

---

## 16. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Descriptor layout drift | Single change set: enum + JSON + C++ + GLSL + reflect |
| Push constant overflow | Shadow/IBL data in UBO only |
| CI asset size / network | Commit minimal baked env; stress toggles off |
| Shadow acne / peter-pan | Tunable bias in ImGui + sane defaults in Progress |
| IBL blows exposure | Default `iblIntensity` < 1.5; use sRGB/linear correct cubemap upload |
| Bindless + batch divergence | Implement both forward frags in same step |
| M7 permutation temptation | Code review: no new `PermutationRegistry.json` entries |

---

## 17. Progress logging

Append to [`s5-ibl-shadows_Progress.md`](s5-ibl-shadows_Progress.md) after each step:

```markdown
## YYYY-MM-DD — Step N

- **Files:** …
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0 (or N/A + reason)
- **Notes:** …
```

**Closeout template:**

```markdown
## Closeout — YYYY-MM-DD

- **Outcome:** …
- **Verification:** commands + exit 0; log: …; screenshots: …
- **Deviations:** none | …
```

---

## 18. Task close checklist (vibe coding)

1. Steps 0–8 done; Closeout in Progress (≤30 lines)
2. `s5-ibl-shadows_Plan.md` → `Status: Closed (YYYY-MM-DD)`
3. Move Plan + Progress → `Docs/Archived/plans/`
4. Add S5 line to `Docs/Archived-Plan.md`; remove/update queue in `Active-Plan.md`
5. Wishlist §S5 → index pointer (shipped)
6. Clear `Docs/README.md` **WIP task** pointer
7. `EngineArchitecture.md` §6 — **yes** (Set 0 lighting bindings + shadow/IBL policy narrative)
8. `SprintOutcomeValidation.md` — only if runbook gaps found (optional)

---

## 19. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Validation: descriptor set layout mismatch | JSON not synced | Re-run reflect; align enum + GLSL |
| Black deferred pass | Barrier / null IBL view | Check dummy cubemap; gbuffer barriers |
| No shadows | Pass not recorded or wrong matrix | Log `lightViewProj`; RenderDoc depth |
| Shadow acne | Bias too low | Increase `myShadowParams.x` |
| Sky not visible | Depth threshold wrong | Adjust 0.999 test; check depth copy range |
| IBL too dark/bright | Linear/sRGB mismatch | Upload with correct `VK_FORMAT` + sampler |
| `Assert-ShaderDrift` | Missing `.spv` commit | `CompileShader.bat` + git add generated |
| Smoke fails on stress | IBL on without assets | Set stress config toggles off |
| Forward/deferred mismatch | Only one frag updated | Edit batch **and** bindless frags |

---

## 20. File checklist (complete touch list)

| Action | Path |
|--------|------|
| **Create** | `Shader/PbrIbl.glsl` |
| **Create** | `Shader/ShadowMap.vert`, `Shader/ShadowMap.frag` (or depth variant) |
| **Create** | `RenderCore/Vk_IblResources.h/.cpp` |
| **Create** | `RenderCore/Vk_ShadowMapPass.h/.cpp` |
| **Create** | `Gfx/Gfx_LightingGlobals.h` |
| **Create** | `Data/Environments/default/*`, `manifest.json` |
| **Create** | `Scripts/Fetch-S5Environment.ps1` *(optional)* |
| Edit | `Shader/DeferredLighting.frag` |
| Edit | `Shader/TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag` |
| Edit | `Shader/PbrDirect.glsl` (forward helper / comments) |
| Edit | `Shader/CompileShader_Glslc.bat` |
| Edit | `RenderCore/Vk_GBufferPass.cpp` (shadow record + log) |
| Edit | `RenderCore/Vk_DeferredLightingPass.cpp/.h` |
| Edit | `RenderCore/Vk_DescriptorSystem.cpp/.h` |
| Edit | `RenderCore/Vk_Enum.h` |
| Edit | `RenderCore/Vk_Core.cpp/.h` (init/destroy/recreate hooks) |
| Edit | `RenderCore/Vk_FrameUniformUploader.cpp` |
| Edit | `RenderCore/Vk_ScenePasses.cpp` (shadow draw reuse if needed) |
| Edit | `Util/Util_Loader.h/.cpp` |
| Edit | `Util/Util_EngineConfig.h/.cpp` |
| Edit | `Util/Util_LightingPanel.cpp` |
| Edit | `Shader/DescriptorContract_LitBatch.json`, `DescriptorContract_LitBindless.json` |
| Edit | `Config/engine.json`, `Config/engine.stress.json`, `Config/engine.benchmark.json` |
| Edit | `VulkanDesktop.vcxproj` + `.filters` (new .cpp modules) |
| Commit | `Shader_Generated/*` (all rebuilt `.spv`) |
| Close | `Docs/EngineArchitecture.md` §6 (Set 0 lighting bindings) |

**Do not edit without reason:** G-buffer formats, `ClusterBuild.comp`, `PermutationRegistry.json`, `GpuMaterialParams` layout.

---

## 21. References

- S4 baseline: [`Archived/plans/s4-pbr-gbuffer_Plan.md`](Archived/plans/s4-pbr-gbuffer_Plan.md)
- S3 FG chain: [`Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md`](Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md)
- Bindless maint: [`Archived/plans/shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance
- Descriptor policy: [`EngineArchitecture.md`](EngineArchitecture.md) §6
- Build/smoke: [`CLI.md`](CLI.md) · `.cursor/rules/vulkan-smoke-test.mdc`
- Epic §C acceptance: [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md)
