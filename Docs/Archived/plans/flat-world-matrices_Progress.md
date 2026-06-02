## Closeout — 2026-06-02

- **Outcome:** Implemented the locked bundle `1B + 2A + 3A + 4A + 5A/5B + 6B`: Gfx owns flat transform source/resolved state, explicit resolve runs before render, SoA API uses world-explicit naming, and RenderCore no longer owns demo base transforms.
- **Verification:** `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` exit `0`; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit `0`; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6 --demo-rotate` exit `0`.
- **Log checks:** `[SCENE] LoadSceneResources completed`; `[RESOURCE] FillInstanceSlab: wrote 9 instance(s)`; `[APP] Smoke dwell reached (6.000000s); requesting exit.`; `[SCENE] UnloadScene: GPU scene resources released`.
- **Deviations:** none.
- **Plan:** [`flat-world-matrices_Plan.md`](flat-world-matrices_Plan.md)
