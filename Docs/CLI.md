# VulkanDesktop — 命令行与配置参考

本文档列出 **CLI 参数**、**`Config/engine.json` 字段** 及其优先级。解析实现：`VulkanDesktop/Util/Util_EngineConfig.cpp`。

**优先级（高 → 低）：** 命令行参数 > `engine.json` > 内置默认值。

---

## 快速示例

```powershell
# 默认：读 Config/engine.json，场景 Data/Scenes/demo.json，从 x64\Debug 或仓库根解析 assetRoot
.\VulkanDesktop.exe

# 指定场景与资源根（cwd 任意）
.\VulkanDesktop.exe --asset-root D:\Code\SiriusEngine_VulkanReboot --scene Data/Scenes/smoke.json

# 开发：关 validation、跑 2 帧后正常退出（验证 UnloadScene）
.\VulkanDesktop.exe --no-validation --smoke-frames 2

# 查看全部开关
.\VulkanDesktop.exe --help
```

运行时还可在 ImGui **「Scene」** 面板中切换 `Data/Scenes/*.json`（进程内 reload，无需重启）。

---

## 命令行参数

| 参数 | 取值 | 作用 |
|------|------|------|
| `--help` / `-h` / `/?` | — | 打印用法到 stderr 并**立即退出**（不创建窗口、不初始化 Vulkan）。 |
| `--config` `<file>` | 路径 | 指定 JSON 配置文件；默认 `<repo>/Config/engine.json`。文件不存在且显式传入 `--config` 时报错。 |
| `--asset-root` `<dir>` | 目录 | **资源根目录**（含 `Data/`、`VulkanDesktop/`）。未指定时：config 里 `assetRoot` 非空则用其值，否则自动向上查找含 `VulkanDesktop.sln` 的目录。 |
| `--scene` `<path>` | 仓库相对路径 | 启动时加载的场景 JSON，默认 `Data/Scenes/demo.json`。仅影响**首次** `LoadSceneResources`；运行中改场景用 ImGui Scene 面板。 |
| `--width` `<n>` | 正整数 | 窗口宽度（像素），覆盖 config。 |
| `--height` `<n>` | 正整数 | 窗口高度（像素），覆盖 config。 |
| `--vsync` | — | 交换链优先 FIFO（垂直同步）。 |
| `--no-vsync` | — | 交换链优先 MAILBOX（更低延迟，可能撕裂）。 |
| `--log-level` `<level>` | `debug` \| `info` \| `warn` \| `error` | 运行时日志最低级别；输出见 `Logs/engine_runtime_log.txt`（或 config 的 `logFile`）。 |
| `--validation` / `--enable-validation` | — | 强制启用 Vulkan 校验层（需已安装，见 [`validation-layers.md`](validation-layers.md)）。 |
| `--no-validation` / `--disable-validation` | — | 强制关闭校验层。 |
| `--demo-rotate` | — | 开启 demo 实体绕 Z 轴旋转（`Gfx_TickDemoSceneTransforms`）。 |
| `--no-demo-rotate` | — | 关闭 demo 旋转。 |
| `--smoke-frames` `<n>` | 正整数 | 渲染 **n** 帧后请求关闭窗口，走完整 `UnloadScene` → `Shutdown`（CI/冒烟用，**不**面向玩家）。 |

**说明：**

- 未知参数会抛错并退出（`Unknown argument: …`）。
- `--help` 在 `UtilEngineConfig::TryEarlyExitFromCli` 中处理，早于 logger 初始化。
- Debug 构建默认倾向开启 validation；Release 默认关闭，均可被 CLI/config 覆盖。

---

## `Config/engine.json`

路径默认：仓库根下 `Config/engine.json`。所有路径字段通常为**仓库相对路径**，经 `UtilLoader::ResolvePath` 解析。

| 字段 | 类型 | 默认 / 说明 |
|------|------|-------------|
| `assetRoot` | string | 空 = 自动探测仓库根；非空则作为资源根（可为绝对或相对 cwd 的路径）。 |
| `scene` | string | 启动场景，默认 `Data/Scenes/demo.json`。 |
| `window.width` / `window.height` | number | 窗口尺寸，默认 1600×1200。 |
| `vsync` | bool | `true` = FIFO；`false` = MAILBOX。 |
| `logLevel` | string | `debug` \| `info` \| `warn` \| `error`。 |
| `logFile` | string | 可选；空则 `Logs/engine_runtime_log.txt`（相对资源根或仓库）。 |
| `assetVerify` | string | `strict`（缺文件则启动失败）或 `warn`（缺文件 `[STARTUP] WARN` 后继续）。 |
| `enableValidationLayers` | bool | 是否启用 Vulkan validation；可被 CLI 覆盖。 |
| `features.demoRotate` | bool | 与 `--demo-rotate` / `--no-demo-rotate` 对应。 |
| `features.runtimeMipmap` | bool | 运行时生成 mipmap（纹理加载路径）；默认 `false`。 |

示例见仓库 [`Config/engine.json`](../Config/engine.json)。

---

## 启动顺序（与参数的关系）

```text
main
  TryEarlyExitFromCli (--help)
  Application::Run
    UtilEngineConfig::Initialize(argc, argv)   ← CLI + engine.json
    UtilLogger::Init
    Gfx_LoadSceneDesc(--scene / config.scene)
    Util_VerifyManifest(..., assetVerify policy)
    InitWindow / InitRenderDevice
    LoadSceneResources(scene)
    loop → Render (+ ImGui Scene 面板可触发 reload)
    UnloadScene → Shutdown
```

---

## 工作目录与 `--asset-root`

- 从 `x64\Debug` 直接运行：若未设 `--asset-root`，会自动找到仓库根，场景与 `Data/` 路径仍有效。
- 从任意 cwd 测试时建议显式：`--asset-root <repo根>`。
- 日志中 `[CONFIG] assetRoot=…` 可确认解析结果。

---

## Agent / CI 测试速查

与 `.cursor/rules/vulkan-smoke-test.mdc`、vibe-coding skill 中 smoke-run 一致：

```powershell
Set-Location x64\Debug
.\VulkanDesktop.exe --no-validation --smoke-frames 2 --scene Data/Scenes/demo.json
.\VulkanDesktop.exe --no-validation --smoke-frames 2 --scene Data/Scenes/smoke.json
```

任务收尾优先用 **`--smoke-frames`**（完整 `UnloadScene`）；仅 `Stop-Process` 杀进程不会走卸载路径。

---

## 相关文档

| 文档 | 内容 |
|------|------|
| [`.cursor/rules/vulkan-smoke-test.mdc`](../.cursor/rules/vulkan-smoke-test.mdc) | Agent 冒烟测试 CLI 速查 |
| [`bootstrap.md`](bootstrap.md) | 新机构建、运行、日志 |
| [`SceneJSON.md`](SceneJSON.md) / [`SceneJSON.en.md`](SceneJSON.en.md) | 场景 JSON 编写 |
| [`validation-layers.md`](validation-layers.md) | Validation 层安装与开关 |
| [`Archived/plans/scene-load_Plan.md`](Archived/plans/scene-load_Plan.md) | 场景加载架构与 Phase D unload |

---

*Last updated: 2026-05-29 (scene-load Phase D, ImGui Scene panel).*
