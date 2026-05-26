# Progress: bindless-v0

## 2026-05-26 — Plan created

- **Plan ref:** SprintPlan S1 Bindless v0.
- **Files:** `Docs/bindless-v0_Plan.md`, `_Progress.md`
- **Verification:** N/A

## 2026-05-26 — Implementation + verify

- **Plan ref:** Goals 1–5; Verification
- **Files:** `Vk_Bindless.*`, `Vk_Core.*`, `Vk_Types.h`, `Vk_Camera.h`, `Vk_Enum.h`, `Vk_DescriptorPolicy.h`, `Vk_ResourceTables.*`, `Gfx_DrawExtract.*`, `TriangleVertex.vert`, `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag`, `CompileShader_Glslc.bat`, docs
- **What changed:** Hybrid bindless (texture array + material SSBO) when indexing available; batch fallback; `materialIndex` in instance slab; sort key carries `materialTableGeneration`.
- **Verification:** MSBuild Debug|x64 exit 0; smoke on dev GPU — extension listed but `runtimeArray=no` → **`materialPath=Batch`** fallback; `materialTableGeneration=1`, 9 draws OK. Bindless record path activates when all three probe flags are yes (discrete GPU typical).
