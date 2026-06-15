# Progress: shadow-audit-fix

## 2026-06-15 — Lighting contract refactor (landed)

- **Decoupled IBL from shadow map:** removed `Pbr_ModulateAmbientForSunShadow` / `PBR_SHADOWED_IBL_SCALE`; shadow affects **direct sun only** (standard PBR).
- **Sun elevation gate:** `Gfx_IsSunElevationValidForShadows` — skip shadow pass + compare when `sunDir.z <= 0.08` (below-horizon sun).
- **PCF:** 3×3 hardware compare taps; `shadowParams.w = 1/2048`.
- **Prefilter:** mip0 only — disabled GPU box mips on prefilter (were not GGX-prefiltered).
- **Files:** `PbrShadow.glsl`, `LightingBindings.glsl`, `DeferredLighting.frag`, `TriangleFrag_Lit*.frag`, `Gfx_LightingMath.h`, `Gfx_LightingGlobals.h`, `Vk_FrameUniformUploader.cpp`, `Vk_ShadowMapPass.cpp`, `Vk_GBufferPass.cpp`, `Vk_ScenePasses.cpp`, `Vk_IblResources.cpp`, `GfxTests_Main.cpp`, `Util_LightingPanel.cpp`.
- **Verification:** `Verify-CI.ps1` — build + GfxTests pass; shader SPIR-V regenerated (commit with code).

## 2026-06-15 — 阳光从下方：遮罩 + 上下半区阴影颠倒

- **关阴影仍有深遮罩：** 不是 shadow map。`shadowParams.z=0` 时 `Pbr_ShadowVisibility` 恒 1、`Pbr_ModulateAmbientForSunShadow` 不压 IBL。遮罩来自 **NdotL 半球截断**（`Pbr_EvalDirect` 在 `NdotL<=0` 返回 0）：`sunDir≈-Z` 时地面法线 +Z 永远无直射，天花板 -Z 全亮；柱/挑檐与亮区交界形成硬边“蒙版”。若 IBL 仍开，地板仅 IBL、顶面直射+IBL，对比更狠。与 AABB / IBL cubemap 遮挡无关。
- **开阴影上下颠倒：** v0 shadow 按 Khronos **头顶太阳** 验收（固定 bias -1.4/-1.7、BACK cull）。光从下方时 light view 反转，深度/compare 与 bias 不匹配 → **sunShadow 大面积错误**；再叠 `PBR_SHADOWED_IBL_SCALE=0.25` 假阴区近黑、假亮区保留 IBL。地板 NdotL 本就为 0，假 sunShadow=1 时仍“相对亮”，易呈现“上半有阴影、下半被照亮”的错觉。
- **Convention：** `mySunlightDirection` = 表面→太阳 = `Pbr_EvalDirect` 的 L；ImGui 负 elevation 即地下太阳。
- **Verify：** 关阴影 + 负 elevation → 地板仍无直射（正常）；Debug Shadow map 在地下太阳时应错乱；恢复默认 sun `(0.25,0.45,0.85)` 恢复正常。

## 2026-06-15 — IBL 阴影区“蒙版”调查

- **结论：** 不是 scene AABB 遮挡 IBL radiance。AABB 仅用于方向光 shadow ortho 拟合（`Gfx_ComputeKhronosDirectionalShadowSetup`）；IBL 运行时按法线/反射方向采样全局 cubemap，与场景几何无关。
- **主因：** `Pbr_ModulateAmbientForSunShadow`（`PBR_SHADOWED_IBL_SCALE=0.25`）在 sun shadow 区把 IBL 压到 25%，直射太阳同时 `radiance *= sunShadow` → 阴影内几乎只剩暗 ambient，边缘与 shadow map 一致，呈“蒙版”感。
- **为何 IBL 开启更明显：** 非阴影区有完整 outdoor HDRI + 直射；阴影区 0.25×IBL + 无 sun → 对比度大。与 IBL off 时 `ambientColor(0.15)*0.25` 的 flat fill 不同。
- **排除：** `Pbr_SunHemisphereIblScale` 已在 audit 中移除；非 narrow-ground radiance / AABB clip。
- **Verification（手动）：** Debug view → Shadow map 对齐暗区；关 Shadows 蒙版消失；调 `PBR_SHADOWED_IBL_SCALE` 或 ImGui IBL intensity 可验证。

## 2026-06-13 — Implement

- **Removed:** `Pbr_SunHemisphereIblScale`, frustum UV reject, `VK_CULL_MODE_BACK_BIT`.
- **Kept:** light eye `focus + sunDir * reach`, scene-scaled ortho + texel snap, viewport compare depth (`z*0.5+0.5`), scaled depth bias.
- **Fixed:** lookAt alternate up when `|dot(+Z, sunDir)| >= 0.85`; **BACK cull** (FRONT cull caused light leak via back-face depth); Khronos fixed depth bias `-1.4` (scaled bias was ~0 for Sponza).
- **Files:** `PbrShadow.glsl`, `DeferredLighting.frag`, `LightingBindings.glsl`, `TriangleFrag_Lit*.frag`, `Vk_ShadowMapPass.cpp`, `Gfx_LightingMath.h`.
- **Verification:** `GfxTests.exe` exit 0; `Verify-Smoke.ps1` exit 0; shader SPIR-V regenerated (commit with code). Manual Sponza + shadow debug view pending user.
