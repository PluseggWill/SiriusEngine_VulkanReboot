# Plan: config-platform-hardening

**Status:** Planned  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P0–P1**  
**Covers recommendations:** #7, #30, #31

## Goal

Replace global config singleton and brittle error handling with **testable, Windows-explicit** startup behavior.

---

## A. EngineConfig instance (#7)

**Landing:**

| Today | Target |
|-------|--------|
| File-static `gAssetRoot`, `gFeatures`, … in `Util_EngineConfig.cpp` | `Util_EngineConfig` struct instance owned by `Application` |
| Free functions `UtilEngineConfig::Get*` | Pass `const Util_EngineConfig&` or `AppContext&` into modules that need config |

**Migration steps:**

1. Introduce struct holding all former globals; `Initialize(argc, argv)` returns or fills instance.
2. Thread instance into `Application` members; deprecate getters one module at a time.
3. `--run-tests` uses default instance without GLFW.

**Done when:** no file-level mutable config globals; tests can construct two configs in one process.

---

## B. Platform honesty (#30)

**Landing:**

- `EngineArchitecture.md` §8 non-goals: **Windows + MSVC + MSBuild only** until product request.
- Replace `_dupenv_s` / `Windows.h` in bindless path with GLFW/env wrapper when touching that file, or document Windows-only block.
- [`Wishlist.md`](Wishlist.md) owns cross-platform; remove duplicate “maybe later” bullets from Active-Plan.

---

## C. Recoverable Vulkan errors (#31)

**Landing:**

| Failure | Behavior |
|---------|----------|
| Swapchain OUT_OF_DATE | existing recreate path |
| `vkQueueSubmit` / present recoverable | log `[VK]`, skip frame, retry — **no throw** |
| Fatal (device lost) | log + graceful `Application` shutdown request |

Replace `throw std::runtime_error` in `Vk_SwapchainHost` present/submit with typed `VkFrameResult` enum consumed by `DrawFrame`.

---

## Verification

- Smoke + intentional resize during run
- Unit test: config precedence CLI over json over defaults

## Touch list

- `Util/Util_EngineConfig.*`, `App/Application.*`
- `RenderCore/Vk_SwapchainHost.*`, `Vk_Core::DrawFrame`
