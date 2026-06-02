# Progress: descriptor-layout-verify

## Closeout — 2026-05-29

- **Outcome:** Verified batch/bindless `VkPipelineLayout` sets 0/1/2 match `Vk_DescriptorPolicy.h` and shaders; added `LogLayoutContract`, rebuild contract in `Vk_DescriptorPolicy.h` + `EngineArchitecture.md` §5.3; fixed stale `Vk_Enum.h` comment and bindless Set 0 bind layout handle.
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-frames 2` exit 0; log: `[DESCRIPTOR] layout contract: path=batch sets=0,1,2`, `[SCENE] UnloadScene: GPU scene resources released`.
- **Deviations:** none
- **Plan:** [`descriptor-layout-verify_Plan.md`](descriptor-layout-verify_Plan.md)
