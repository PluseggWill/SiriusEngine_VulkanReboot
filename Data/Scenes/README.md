# Data/Scenes

Scene files for VulkanDesktop (JSON v1).

| File | Purpose |
|------|---------|
| [`demo.json`](demo.json) | Default Kenney camp + viking/monkey demo; **Stage 1 forward baseline** ([`forward-stage1.md`](../../Docs/forward-stage1.md) §1) |
| [`smoke.json`](smoke.json) | Minimal load test (single viking entity) |

**Authoring:** [EN](../../Docs/SceneJSON.en.md) · [中文](../../Docs/SceneJSON.md)

**Run:**

```text
VulkanDesktop.exe --scene Data/Scenes/demo.json
VulkanDesktop.exe --scene Data/Scenes/smoke.json
```

Default scene path if `--scene` is omitted: `Data/Scenes/demo.json` (`kGfxDefaultSceneLogicalPath`).
