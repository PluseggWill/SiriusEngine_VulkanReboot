# VulkanDesktop

Vulkan learning / engine reboot sandbox (SiriusEngine).

## Run (Windows)

Build `VulkanDesktop.sln` (Debug|x64), run `x64\Debug\VulkanDesktop.exe`.

## Debug camera

| Input | Action |
|-------|--------|
| W / S / A / D | Move on horizontal plane (view-relative) |
| Q / E | Down / up (world Z) |
| Hold **RMB** + mouse | Look around |
| ImGui **Camera** panel | Move speed, mouse sensitivity |

Details: `VibeCoding/fps-camera_Plan.md`. Roadmap: `VibeCoding/TODOList.md`. Architecture notes: `Logs/EngineArchitecture.md` §3.1.