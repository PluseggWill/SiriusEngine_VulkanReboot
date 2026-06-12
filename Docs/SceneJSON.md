# 场景 JSON 编写说明（v1）

> **English:** [`SceneJSON.en.md`](SceneJSON.en.md)  
> **格式版本：** `"version": 1`（引擎当前仅接受此版本）  
> **解析实现：** `Gfx_LoadSceneDesc`（`VulkanDesktop/Gfx/Gfx_SceneLoader.cpp`）  
> **运行时灌入：** `Gfx_SceneApply`（manifest / SoA / LOD）  
> **示例文件：** [`Data/Scenes/demo.json`](../Data/Scenes/demo.json)  
> **任务与依赖：** [`Archived/plans/scene-load_Plan.md`](Archived/plans/scene-load_Plan.md) → Handoff

---

## 1. 文件放哪、怎么跑

| 项 | 规则 |
|----|------|
| **目录** | `Data/Scenes/<name>.json`（repo 相对路径） |
| **默认场景** | `Data/Scenes/sponza.json`（需 fetch；CI 冒烟用 [`stress.json`](../Data/Scenes/stress.json)；Stage 1 golden 仍用 [`demo.json`](../Data/Scenes/demo.json)） |
| **CLI** | `VulkanDesktop.exe --scene Data/Scenes/your_scene.json` |
| **资产根** | 所有 `path` 相对 **asset root**（`Config/engine.json` 的 `assetRoot` 或 `--asset-root`），由 `UtilLoader::ResolvePath` 解析 |
| **启动校验** | 启动前 `Util_VerifyManifest` 检查 manifest；`Config/engine.json` 的 `"assetVerify": "strict"`（缺文件退出）或 `"warn"`（缺文件 `[STARTUP] WARN` 后继续） |

**路径写法示例：**

- 模型/纹理：`Data/Models/…`, `Data/Textures/…`
- 着色器 SPIR-V：`VulkanDesktop/Shader_Generated/TriangleVert.spv`（先编译 GLSL）

---

## 2. 顶层结构（推荐顺序）

```json
{
  "version": 1,
  "name": "my_scene",
  "shaders": { },
  "logicalMeshes": [ ],
  "meshes": [ ],
  "textures": [ ],
  "materials": [ ],
  "entities": [ ]
}
```

JSON 对象内键顺序不影响解析；上表顺序便于人工阅读和对照 `demo.json`。

---

## 3. 各段说明

### 3.1 `version`（必填）

- **类型：** 无符号整数  
- **当前仅支持：** `1`  
- 其它值 → 启动时报 `[SCENE] Unsupported scene version …`

### 3.2 `name`（可选）

- **类型：** 字符串  
- 仅用于日志（`[SCENE] Parsed scene v1 name='…'`）

### 3.3 `shaders`（运行时需要 `lit`）

对象：键 = shader 逻辑名，值 = SPIR-V 路径对。

```json
"shaders": {
  "lit": {
    "vert": "VulkanDesktop/Shader_Generated/TriangleVert.spv",
    "frag": "VulkanDesktop/Shader_Generated/TrianglePix.spv"
  }
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `vert` | 是 | 顶点着色器，repo 相对路径 |
| `frag` | 是 | 片元着色器，repo 相对路径 |

**约定：** 当前 `Vk_Core` 在 `InitVulkan` 中写死使用 **`lit`** 这一对路径创建主管线。所有 `materials[].shader` 应引用已在此声明的 id（通常为 `"lit"`）。

### 3.4 `logicalMeshes`（有实体时 practically 必填）

定义 **逻辑网格**（SoA 上的 `logicalMeshId`）与 **LOD 链**（物理 mesh id 列表）。

```json
{
  "id": "tree",
  "lodMeshes": ["kenney_tree_detailed", "kenney_tree_simple"],
  "lodDistances": [14.0]
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `id` | 是 | 逻辑名；`entities[].logicalMesh` 引用此字符串 |
| `lodMeshes` | 是 | 字符串数组，元素为 `meshes[].id`；索引 0 = 最近（最高细节） |
| `lodDistances` | 否 | 浮点数组；`lodDistances[i]` 是从 LOD i 切换到 i+1 的距离阈值（米，世界空间） |

**规则：**

- `lodDistances` 条数必须 **小于** `lodMeshes` 条数（N 个 mesh 最多 N−1 个阈值）。  
- 仅一个 mesh 时可省略 `lodDistances`（始终 LOD0）。  
- `lodMeshes` 中的 id 必须在 `meshes` 里已定义。  
- **逻辑 id → 运行时整数下标** = `logicalMeshes` **数组顺序**（从 0 开始），与 id 字符串无关。

**LOD 行为：** 见 [`Data/LOD.md`](../Data/LOD.md)（含 15% 迟滞）。

### 3.5 `meshes`（必填，若场景要可见几何）

```json
{ "id": "viking_room", "path": "Data/Models/viking_room.obj" }
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `id` | 是 | 物理 mesh 名；被 `logicalMeshes[].lodMeshes` 引用 |
| `path` | 是 | OBJ 路径（repo 相对） |

**运行时 id：** `meshes` 数组中的 **下标**（0, 1, 2, …）即资源表 mesh id。  
**保持与 demo 一致时：** 顺序须与 `demo.json` 一致，否则需同步改 `logicalMeshes` 引用。

### 3.6 `textures`

```json
{ "id": "viking_albedo", "path": "Data/Textures/viking_room.png" }
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `id` | 是 | 材质 `texture` 字段引用 |
| `path` | 是 | 图像路径（png/jpg 等） |

**运行时 id：** 数组下标 = 纹理表 id。

### 3.7 `materials`

```json
{
  "id": "mat_transparent",
  "shader": "lit",
  "texture": "viking_albedo",
  "alpha": 0.35,
  "transparent": true
}
```

| 字段 | 必填 | 默认 | 说明 |
|------|------|------|------|
| `id` | 是 | — | 实体 `material` 引用 |
| `shader` | 是 | — | 须存在于 `shaders` |
| `texture` | 是 | — | 须存在于 `textures` |
| `baseColor` | 否 | `[1,1,1,1]` | 3 或 4 个 float（RGB 或 RGBA），写入 `GpuMaterialParams.baseColorFactor` |
| `roughness` | 否 | `0.5` | PBR 预留（当前着色仍用环境 UBO 高光参数） |
| `metallic` | 否 | `0.0` | PBR 预留 |
| `alpha` | 否 | `1.0` | 材质 alpha（透明材质常用 &lt; 1） |
| `alphaMode` | 否 | `opaque` | `opaque` / `mask` / `blend`；与 `transparent: true` 同时出现时未写 `alphaMode` 时默认为 `blend` |
| `transparent` | 否 | `false` | `true` → 走透明 pass（与 `renderFlags: transparent` 一致） |

**运行时 id：** 数组下标 = 材质表 id。

### 3.8 `entities`（场景里放什么、放哪）

```json
{
  "logicalMesh": "tree",
  "material": "mat_grass",
  "transform": [4, 0, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, -6, 0, -1, 1],
  "renderFlags": "opaque",
  "layerMask": 4294967295,
  "lodBias": 0.0
}
```

| 字段 | 必填 | 默认 | 说明 |
|------|------|------|------|
| `logicalMesh` | 是 | — | 对应 `logicalMeshes[].id` |
| `material` | 是 | — | 对应 `materials[].id` |
| `transform` | 是 | — | **16 个 float**，见下文 |
| `renderFlags` | 否 | `"opaque"` | `"opaque"` 或 `"transparent"`（也可用无符号整数枚举） |
| `layerMask` | 否 | 全 1 | 剔除用 layer 掩码 |
| `lodBias` | 否 | `0` | 加减到 LOD 距离（米） |

**层级：** v1 **无** parent/child；每个实体一根 **世界矩阵**（扁平场景）。

---

## 4. `transform` 矩阵约定（重要）

- **16 个数字**，**列主序**（column-major），与 **GLM** `glm::mat4` / `glm::value_ptr` 一致。  
- 即矩阵按列存入数组：`[col0.x, col0.y, col0.z, col0.w, col1.x, …, col3.w]`。

**纯平移示例**（世界位置 (-4, 0, 0)）：

```json
"transform": [1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  -4, 0, 0, 1]
```

**平移 + 均匀缩放**（先 scale 再 translate，与 `glm::translate * glm::scale` 一致）：

```text
T * S  →  对角线为 scale，最后一列前三个分量为平移
```

**在 DCC / 脚本里生成：**

- Blender 等导出时注意列主序；不要按行主序直接粘贴。  
- 可用 Python + GLM 或引擎侧 `ComputeDemoModelMatrix` 同类逻辑预计算后写入 JSON。

**运行时：** `ENABLE_ROTATE` 为 true 时，demo 仍会对 **base transform**（来自 JSON）做 Z 轴旋转动画（`ApplyDemoTransformAnimation`）。

---

## 5. 引用完整性检查清单

编写或改场景后，按顺序自检：

1. **`version`: 1**  
2. **`shaders.lit`** 存在且 SPIR-V 已编译  
3. 每个 **`logicalMeshes[].lodMeshes`** 中的 id ∈ **`meshes[].id`**  
4. 每个 **`entities[].logicalMesh`** ∈ **`logicalMeshes[].id`**  
5. 每个 **`entities[].material`** ∈ **`materials[].id`**  
6. 每个 **`materials[].texture`** ∈ **`textures[].id`**，**`shader`** ∈ **`shaders`**  
7. **`lodDistances.length` < `lodMeshes.length`**（若有 distances）  
8. 各 section 内 **`id` 不重复**  
9. 从 repo 根或 `x64\Debug` 带 `--asset-root` 跑一遍，看 `[STARTUP] OK` 与 `[SCENE] Parsed … entities=N`

---

## 6. 运行时数字 id 对照（demo 场景）

以下为 **`demo.json` 当前数组顺序**；若你增删或重排条目，下表会变化。

**`logicalMeshes` 下标 → id**

| 下标 | id |
|------|-----|
| 0 | viking |
| 1 | monkey |
| 2 | tree |
| 3 | rock |
| 4 | campfire |
| 5 | tent |
| 6 | stump |

**`meshes` 下标 → id**（资源表 physical mesh id）

| 下标 | id |
|------|-----|
| 0 | viking_room |
| 1 | monkey |
| 2 | kenney_tree_detailed |
| 3 | kenney_tree_simple |
| 4 | kenney_rock_large |
| 5 | kenney_campfire |
| 6 | kenney_tent |
| 7 | kenney_stump |

**`materials` 下标 → id**

| 下标 | id |
|------|-----|
| 0 | mat_viking |
| 1 | mat_monkey |
| 2 | mat_transparent |
| 3 | mat_rock |
| 4 | mat_grass |
| 5 | mat_metal |
| 6 | mat_wood |

---

## 7. 新建场景的最小步骤

1. 复制 `Data/Scenes/demo.json` → `Data/Scenes/my_scene.json`。  
2. 改 `name`；按需删减 `entities` / 资源表。  
3. 新增 mesh 时：先写 `meshes`，再在 `logicalMeshes` 的 `lodMeshes` 里引用。  
4. 新增实体：写 `logicalMesh` + `material` + `transform`。  
5. 编译着色器（若改了 GLSL）。  
6. 运行：  
   `VulkanDesktop.exe --scene Data/Scenes/my_scene.json`  
7. 查日志：`[STARTUP]`、`[SCENE]`、`[RESOURCE-TABLE]`、`[EXTRACT] entities=… draws=…`。

---

## 8. 常见错误与日志

| 现象 | 可能原因 |
|------|----------|
| `[SCENE] Unsupported scene version` | `version` ≠ 1 |
| `[SCENE] Missing transform` | 实体缺 `transform` 或不是 16 个数 |
| `[SCENE] Unknown logicalMesh` | `entities.logicalMesh` 未在 `logicalMeshes` 定义 |
| `[SCENE] Material '…' references unknown texture` | 材质纹理 id 拼写错误 |
| `[SCENE] logicalMeshes[].lodMeshes` 相关 | 未写 `lodMeshes` 或引用了不存在的 mesh id |
| `[STARTUP] Missing required file` | `path` 错误或 asset root 不对 |
| 画面全空 / draws=0 | 无 `entities` 或剔除/层掩码问题 |

---

## 9. 与后续工作的关系

| 能力 | v1 JSON | 计划 |
|------|---------|------|
| 场景卸载 / 热切换 | **支持** — ImGui Scene 面板进程内 reload；启动用 `--scene` | Phase D 完成 |
| 缺资源 warn 继续 | **启动时** — `engine.json` `assetVerify: warn` | Phase D2 |
| 场景层级 parent | 不支持 | scene-load non-goals，以后版本 |
| 多 shader / permutation | 仅声明多组；运行时用 `lit` | S2 shader stack |
| 相机 / 多视图 | 不在 JSON | S2 multi-view（见 Active-Plan） |

---

## 10. 相关代码索引

| 用途 | 文件 |
|------|------|
| 解析 | `Gfx_SceneLoader.cpp` |
| 类型定义 | `Gfx_SceneDesc.h` |
| Manifest / SoA / LOD | `Gfx_SceneApply.cpp` |
| 启动校验 | `Util_AssetManifest.cpp` → `Util_VerifyManifest` |
| 入口 | `VulkanDesktop.cpp` |
| GPU 加载 | `Vk_Core.cpp` → `InitVulkan` → `LoadFromManifest` |
