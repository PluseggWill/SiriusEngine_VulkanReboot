# Progress: swapchain-recreation

## 2026-06-08 — P1 + P2b implementation

- **Files:** `Vk_SwapchainHost.{h,cpp}`, `Vk_DataStruct.h`
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; P1-A1/A2 grep OK

## Closeout — 2026-06-08

- **Outcome:** Khronos acquire-retry (OUT_OF_DATE/SUBOPTIMAL → Recreate → one retry); `createInfo.oldSwapchain` handoff in `Recreate`; fence reset unchanged in `DrawFrameGpu`.
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; `powershell -File Scripts/Verify-Smoke.ps1` exit 0; log: `[SWAPCHAIN] Swapchain recreation completed.` during smoke (present-path resize).
- **Deviations:** P3-V1 manual 10s resize soak not run in agent session (optional per plan). `vkDeviceWaitIdle` kept in `Recreate` (P2b baseline).
