# Progress: shader-bindless-policy

## 2026-06-09 — Phase 1 decision + maint docs

- **Decision:** Option **A** (Dogfood bindless).
- **Docs:** Maintenance contract in Plan; reminders in `Active-Plan.md` P1, `Wishlist.md`, `EngineArchitecture.md` §6.
- **Code:** `POLICY_BINDLESS` comments in `Vk_Bindless.cpp`, `Vk_ScenePasses.cpp`, `Vk_DescriptorSystem.cpp`.

## Closeout — 2026-06-09

- **Outcome:** Option A Phase 3 landed — #14 RenderDoc gate (`BINDLESS_RENDERDOC_OK=1`), #15 unified `RecordPassDrawsFromPacket`, #17 M7 perm freeze in `Gfx_ShaderPermutation` + glslc comment. #18 layout codegen remains Wishlist.
- **Files:** `Vk_Bindless.cpp`, `Vk_ScenePasses.cpp/.h`, `Gfx_ShaderPermutation.cpp/.h`, `CompileShader_Glslc.bat`, `Docs/Platform.md`.
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; log `materialPath=Bindless` on RTX 4070 Ti SUPER.
- **Deviations:** none.
