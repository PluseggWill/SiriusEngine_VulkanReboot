# Plan: rhi-swapchain-create (RHI-B1)

**Status:** Closed (2026-06-09)  
**Parent:** [`vulkan-rhi-hardening-epic_Plan.md`](vulkan-rhi-hardening-epic_Plan.md) §RHI-B1 · [`Active-Plan.md`](Active-Plan.md) P1  
**Hardening:** #35

## Problem

`CreateSwapChain` hardcodes `VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR` and `minImageCount+1` only; `Recreate` has no extent precheck logging before heavy rebuild.

## Non-goals

- B2 three-layer `Recreate` split
- Skip pipeline rebuild on suboptimal-only (B2 feeds)

## Touch list

| File | Change |
|------|--------|
| `Vk_SwapchainHost.h` | `NeedsSwapchainRebuild`; frames-in-flight invariant comment |
| `Vk_SwapchainHost.cpp` | compositeAlpha chain, image count, create log, Recreate precheck |

## Steps

1. `ChooseCompositeAlpha` + `ChooseSwapchainImageCount` helpers in `CreateSwapChain`.
2. Log `imageCount=` and `compositeAlpha=` at create.
3. `NeedsSwapchainRebuild` + log in `Recreate` before `vkDeviceWaitIdle`.
4. Verify: `Verify-CI.ps1` + `Verify-Smoke.ps1`.

## Verification

| ID | Criterion |
|----|-----------|
| B1-V1 | Create log includes `imageCount=` and `compositeAlpha=` |
| B1-V2 | Manual resize smoke (deferred; G0-smoke covers baseline) |
| B1-V3 | `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 |
