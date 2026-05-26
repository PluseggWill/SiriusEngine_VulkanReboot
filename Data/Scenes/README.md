# Data/Scenes

Scene files for VulkanDesktop (JSON v1).

| File | Purpose |
|------|---------|
| [`demo.json`](demo.json) | Default Kenney camp + viking/monkey demo |
| *(planned)* `smoke.json` | Minimal load test (scene-load Phase D3) |

**Authoring:** [EN](../../Docs/SceneJSON.en.md) · [中文](../../Docs/SceneJSON.md)

**Run:**

```text
VulkanDesktop.exe --scene Data/Scenes/demo.json
```

Default scene path if `--scene` is omitted: `Data/Scenes/demo.json` (`kGfxDefaultSceneLogicalPath`).
