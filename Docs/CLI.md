# VulkanDesktop — 命令行与配置参考

本文档列出 **CLI 参数**、**`Config/engine.json` 字段** 及其优先级。解析实现：`VulkanDesktop/Util/Util_EngineConfig.cpp`。

**优先级（高 → 低）：** 命令行参数 > `engine.json` > 内置默认值。

---

## 快速示例

```powershell
# 默认：读 Config/engine.json，场景 Data/Scenes/stress.json，从 x64\Debug 或仓库根解析 assetRoot
.\VulkanDesktop.exe

# 指定场景与资源根（cwd 任意）
.\VulkanDesktop.exe --asset-root D:\Code\SiriusEngine_VulkanReboot --scene Data/Scenes/smoke.json

# 开发：关 validation、默认场景加载后主循环至少 6s 后正常退出（验证 UnloadScene）
.\VulkanDesktop.exe --no-validation --smoke-seconds 6

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
| `--scene` `<path>` | 仓库相对路径 | 启动时加载的场景 JSON，默认 `Data/Scenes/stress.json`。仅影响**首次** `LoadSceneResources`；运行中改场景用 ImGui Scene 面板。 |
| `--width` `<n>` | 正整数 | 窗口宽度（像素），覆盖 config。 |
| `--height` `<n>` | 正整数 | 窗口高度（像素），覆盖 config。 |
| `--vsync` | — | 交换链优先 FIFO（垂直同步）。 |
| `--no-vsync` | — | 交换链优先 MAILBOX（更低延迟，可能撕裂）。 |
| `--log-level` `<level>` | `debug` \| `info` \| `warn` \| `error` | 运行时日志最低级别；输出见 `Logs/engine_runtime_log.txt`（或 config 的 `logFile`）。 |
| `--validation` / `--enable-validation` | — | 强制启用 Vulkan 校验层（需已安装，见 [`validation-layers.md`](validation-layers.md)）。 |
| `--no-validation` / `--disable-validation` | — | 强制关闭校验层。 |
| `--demo-rotate` | — | 开启 demo 实体绕 Z 轴旋转（`Gfx_TickDemoSceneTransforms`）。 |
| `--no-demo-rotate` | — | 关闭 demo 旋转（与 `features.demoRotate: false` 默认一致）。 |
| `--lod-enabled` | — | 开启 CPU mesh LOD（draw stream；ImGui **CPU LOD** 为会话覆盖）。 |
| `--no-lod-enabled` | — | 关闭 CPU mesh LOD（与 `features.lodEnabled: false` 默认一致）。 |
| `--smoke-frames` `<n>` | 正整数 | 渲染 **n** 帧后（与 `--smoke-seconds` 同时设置时需**同时**满足）请求关闭，走完整 `UnloadScene` → `Shutdown`。 |
| `--smoke-seconds` `<s>` | 正数 | 场景 `LoadSceneResources` 完成、进入主循环后至少运行 **s** 秒再请求关闭（任务收尾冒烟**推荐**）。 |
| `--perf-log` `<path>` | 文件路径 | 每个完成的 `DrawFrame` 追加一行 JSONL（`schemaVersion` 1：`frameIndex`, `frameMs`, `drawCalls`, `visibleDraws`, `activeViews`, `materialPath`）。`visibleDraws` 为当前帧各视图之和。路径相对 asset root；配合 `engine.benchmark.json`（`vsync: false`）。汇总：`Scripts/Perf-JsonlSummary.ps1`。 |
| `--descriptor-layout-mismatch-test` | — | 开发用：在 `InitDeviceLayouts` 后对 Set 2 做一次故意的 `vkUpdateDescriptorSets` 类型不匹配，供 validation 报错（**必须**配合 `--validation`）。 |
| `--renderdoc` | — | 启用 RenderDoc 运行时接入（启动门控）。仅在该参数存在时被动探测已注入进程的 `renderdoc.dll`（`GetModuleHandle`），并启用 draw/pass debug label 输出路径。推荐从 RenderDoc UI 启动程序。 |
| `--gpu-cull` / `--no-gpu-cull` | — | 启用 compute frustum cull → per-entity-slot `VkDrawIndexedIndirectCommand`（`EntityCull.comp`）；默认 **off**。`Verify-Smoke.ps1` 第二遍自动加 `--gpu-cull`（M2 dogfood）；可用 `-SkipGpuCull` 跳过。 |
| `--legacy-direct-draw` | — | 调试用：每 draw `vkCmdDrawIndexed`（非 indirect）；与 `--gpu-cull` 互斥于验收路径。 |

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
| `scene` | string | 启动场景，默认 `Data/Scenes/stress.json`。 |
| `window.width` / `window.height` | number | 窗口尺寸，默认 1600×1200。 |
| `vsync` | bool | `true` = FIFO；`false` = MAILBOX。 |
| `logLevel` | string | `debug` \| `info` \| `warn` \| `error`。 |
| `logFile` | string | 可选；空则 `Logs/engine_runtime_log.txt`（相对资源根或仓库）。 |
| `assetVerify` | string | `strict`（缺文件则启动失败）或 `warn`（缺文件 `[STARTUP] WARN` 后继续）。 |
| `enableValidationLayers` | bool | 是否启用 Vulkan validation；可被 CLI 覆盖。 |
| `features.demoRotate` | bool | 默认 `false`；与 `--demo-rotate` / `--no-demo-rotate` 对应（`Gfx_TickDemoSceneTransforms`）。 |
| `features.lodEnabled` | bool | 默认 `false`；CPU draw stream 是否做 mesh LOD；与 `--lod-enabled` / `--no-lod-enabled` 对应；ImGui **Render Debug → CPU LOD** 为会话覆盖。 |
| `features.runtimeMipmap` | bool | 运行时生成 mipmap（纹理加载路径）；默认 `false`。 |
| `renderPreset` | string | `ForwardLit`、`ForwardLitAlphaClip`（Stage 1）；`HybridDeferred`（Stage 2 slice 1：G-buffer + albedo composite）。CLI `--render-preset` 可覆盖。 |
| `shaderPermutation` | string | 可选；显式 registry 名（如 `lit`）时优先于 `renderPreset`。 |

示例见仓库 [`Config/engine.json`](../Config/engine.json)。

**基准捕获配置（Stage 1 golden / perf）：** [`Config/engine.benchmark.json`](../Config/engine.benchmark.json) — 见 [`forward-stage1.md`](forward-stage1.md) §1。

**压力 / 功能测试场景：** [`Config/engine.stress.json`](../Config/engine.stress.json) + [`Data/Scenes/stress.json`](../Data/Scenes/stress.json) — 河谷聚落（地面、北崖瀑布、河道、石桥、东岸长屋、西岸森林），~108 实体、`lodEnabled: true`；`Verify-Smoke.ps1` / G0-smoke 默认使用此组合（两遍：CPU indirect + `--gpu-cull`）。最小加载仍可用 [`smoke.json`](../Data/Scenes/smoke.json)。

**HybridDeferred dogfood（slice 1）：** `--render-preset HybridDeferred`；G-buffer 录制当前为 **batch-only**（bindless 自动回退 ForwardLit）。手动验证时设 `FORCE_MATERIAL_BATCH=1`（见 [`Platform.md`](Platform.md)）。

```powershell
.\VulkanDesktop.exe --asset-root <repo> --config <repo>\Config\engine.benchmark.json --no-validation
.\VulkanDesktop.exe --asset-root <repo> --config <repo>\Config\engine.stress.json --no-validation --perf-log Logs/perf_stress.jsonl
```

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

| 场景 | 规则 |
|------|------|
| **本地开发**（`x64\Debug`） | 未指定时仍可通过 `VulkanDesktop.sln` 向上探测仓库根（S0 行为；P0 起文档标为 deprecated，建议显式 `--asset-root`）。 |
| **CI / Agent / `Verify-Bootstrap`** | **必须** `--asset-root <repo根>`；见 [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md) Phase 2–3。 |
| **未来可选** | 环境变量 `SIRIUS_STRICT_ASSET_ROOT=1` 时无 CLI/config 则启动失败（计划项，未实现前勿依赖）。 |

- 日志中 `[CONFIG] assetRoot=…` 可确认解析结果。

---

## Agent / CI 测试速查

与 [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md) Phase 2、`.cursor/rules/vulkan-smoke-test.mdc` 一致。

**CI / 任务收尾（推荐，任意 cwd）：**

```powershell
$Repo = "<repo-root>"
& "$Repo\x64\Debug\VulkanDesktop.exe" `
  --asset-root $Repo `
  --config "$Repo\Config\engine.benchmark.json" `
  --scene Data/Scenes/smoke.json `
  --no-validation --smoke-frames 120 --smoke-seconds 6
pwsh -File "$Repo\Scripts\Assert-SmokeLog.ps1" -RepoRoot $Repo
```

**本地快速（仍建议 `--asset-root`）：**

```powershell
Set-Location x64\Debug
.\VulkanDesktop.exe --asset-root <repo-root> --no-validation --smoke-seconds 6
.\VulkanDesktop.exe --asset-root <repo-root> --no-validation --smoke-frames 2 --scene Data/Scenes/smoke.json
```

任务收尾优先 **`--smoke-seconds 6`** + 完整卸载日志；仅 `Stop-Process` 不会走 `UnloadScene`。实现 P0 后本地可用 `pwsh -File Scripts/Verify-CI.ps1` 镜像 G0。

**WSI 缩放冒烟（可选，RHI-B4）：** 在 G0 构建后运行 `powershell -File Scripts/Verify-ResizeSmoke.ps1`。脚本启动 `demo.json`，通过 Win32 `SetWindowPos` 触发 `[SWAPCHAIN] rebuild layer=wsi`；找不到窗口时降级为无程序化缩放（需手动拖拽验证）。CI 可 soft-fail。

---

## 相关文档

| 文档 | 内容 |
|------|------|
| [`.cursor/rules/vulkan-smoke-test.mdc`](../.cursor/rules/vulkan-smoke-test.mdc) | Agent 冒烟测试 CLI 速查 |
| [`bootstrap.md`](bootstrap.md) | 新机构建、运行、日志 |
| [`SceneJSON.md`](SceneJSON.md) / [`SceneJSON.en.md`](SceneJSON.en.md) | 场景 JSON 编写 |
| [`validation-layers.md`](validation-layers.md) | Validation 层安装与开关 |
| [`Archived/plans/scene-load_Plan.md`](Archived/plans/scene-load_Plan.md) | 场景加载架构与 Phase D unload |
| [`Archived/plans/ci-verification_Plan.md`](Archived/plans/ci-verification_Plan.md) | G0 CI、GfxTests、冒烟断言脚本 |

---

*Last updated: 2026-06-02 (P0 CI contract; automation requires `--asset-root`).*
