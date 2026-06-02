# Platform inventory — VulkanDesktop

**Policy:** Supported target is **Windows 10+ x64**, **MSVC**, **MSBuild** only until a Wishlist port epic lands. Cross-platform work is tracked in [`Wishlist.md`](Wishlist.md), not duplicated in [`Active-Plan.md`](Active-Plan.md).

| Path | Win32 / platform API | Why Windows-only | Port owner |
|------|----------------------|------------------|------------|
| `VulkanDesktop/App/Application.cpp` | GLFW window/input | Desktop shell; GLFW is portable but product ships Windows-only | Wishlist |
| `VulkanDesktop/RenderCore/Vk_Bindless.cpp` | `_dupenv_s( "FORCE_MATERIAL_BATCH" )` | S7 lab / RenderDoc stability override | Wishlist |
| `VulkanDesktop/RenderCore/Vk_RenderDoc.cpp` | `LoadLibraryW`, `GetProcAddress` | RenderDoc DLL injection path | Wishlist |
| `VulkanDesktop/GfxTests/GfxTests_Main.cpp` | `GetModuleFileNameW` | Locate repo root from test exe | Wishlist |

No other `#include <Windows.h>` under `VulkanDesktop/` as of config-platform-hardening closeout.
