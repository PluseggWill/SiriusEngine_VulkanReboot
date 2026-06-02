# Progress: pipeline-dynamic-state-wire

## Closeout — 2026-06-01

- **Outcome:** Scene lit pipelines use dynamic viewport, scissor, and line width; `SetGraphicsDynamicState` sets all three once per scene render pass; legacy `Pipeline_DynamicStateCreateInfo` removed. Policy documented in `EngineArchitecture.md` §5.2.
- **Verification:** `.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit **0**; log: `dynamicState count=3 wired`, `[SCENE] LoadSceneResources completed`, `[SCENE] UnloadScene: GPU scene resources released`.
- **Deviations:** none.
- **Plan:** [`pipeline-dynamic-state-wire_Plan.md`](pipeline-dynamic-state-wire_Plan.md)
