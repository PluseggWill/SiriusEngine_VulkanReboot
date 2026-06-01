# Progress: image-queue-sharing

## Closeout — 2026-06-01

- **Outcome:** Added `VkInit::FillImageSharingMode` and applied it to both `Vk_ResourceContext::CreateImage` and `Vk_Core::CreateImage`. Same-family devices stay `EXCLUSIVE`; dedicated transfer family uses `CONCURRENT` for images, aligned with buffer policy.
- **Verification:** `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` exit 0; `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0.
- **Log checks:** `[VULKAN] Queue families: graphics=0 transfer=0 (image/buffer sharing=EXCLUSIVE)`; `[SCENE] LoadSceneResources completed.`; `[SCENE] UnloadScene: GPU scene resources released.`
- **Deviations:** none.
- **Plan:** [`image-queue-sharing_Plan.md`](image-queue-sharing_Plan.md)
