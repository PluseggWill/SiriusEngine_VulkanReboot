# Plan: image-queue-sharing

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Closed (2026-06-01)  
**Active-Plan:** `- [ ] **Image queue sharing** when transfer ≠ graphics family.`

## Problem

`CreateImage` paths used hardcoded `VK_SHARING_MODE_EXCLUSIVE` while texture upload touches the same image across graphics and transfer queues (`TransitionImageLayout` on graphics, `CopyBufferToImage` on transfer, final transitions on graphics).

## Goals

1. Match image sharing policy with buffer policy: `CONCURRENT` when `graphicsFamily != transferFamily`, otherwise `EXCLUSIVE`.
2. Apply policy in both allocation sites: `Vk_ResourceContext::CreateImage` and `Vk_Core::CreateImage`.
3. Add one startup log for queue families and effective sharing mode.
4. Clarify `VkInit::ImageCreateInfo` comment that EXCLUSIVE is default and may be overridden.

## Non-goals

- No explicit queue-family ownership transfer barriers (`srcQueueFamilyIndex`/`dstQueueFamilyIndex`) for this task.
- No wide loader/swapchain/barrier refactor.
- No shader/descriptor/scene schema changes.

## Implementation plan

- [x] P1 — Add `VkInit::FillImageSharingMode(...)`.
- [x] P2 — Use helper in `Vk_ResourceContext::CreateImage`.
- [x] P3 — Use helper in `Vk_Core::CreateImage`.
- [x] P4 — Add queue-family sharing log in queue-family init.
- [x] P5 — Update image-create comments.
- [x] P6 — Build + smoke-run verification.
- [x] P7 — Closeout and archive.

## Verification

- `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m` → exit 0
- `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` → exit 0
- Log checks:
  - `[VULKAN] Queue families: graphics=0 transfer=0 (image/buffer sharing=EXCLUSIVE)`
  - `[SCENE] LoadSceneResources completed.`
  - `[SCENE] UnloadScene: GPU scene resources released.`

## Risks and rollback

- Risk: duplicate queue family indices with `CONCURRENT` are invalid.
- Mitigation: helper keeps `EXCLUSIVE` when indices are equal.
- Rollback: revert helper and two image call sites plus startup log.
