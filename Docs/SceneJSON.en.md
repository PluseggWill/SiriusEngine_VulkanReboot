# Scene JSON authoring guide (v1)

> **Also available in Chinese:** [`SceneJSON.md`](SceneJSON.md)  
> **Format version:** `"version": 1` (only version accepted today)  
> **Parser:** `Gfx_LoadSceneDesc` (`VulkanDesktop/Gfx/Gfx_SceneLoader.cpp`)  
> **Runtime hydration:** `Gfx_SceneApply` (manifest / SoA / LOD)  
> **Example:** [`Data/Scenes/demo.json`](../Data/Scenes/demo.json)  
> **Task context:** [`scene-load_Plan.md`](scene-load_Plan.md) → Handoff

---

## 1. Where files live and how to run

| Item | Rule |
|------|------|
| **Location** | `Data/Scenes/<name>.json` (repo-relative path) |
| **Default scene** | `Data/Scenes/demo.json` |
| **CLI** | `VulkanDesktop.exe --scene Data/Scenes/your_scene.json` |
| **Asset root** | Every `path` is repo-relative to **asset root** (`assetRoot` in `Config/engine.json` or `--asset-root`), resolved by `UtilLoader::ResolvePath` |
| **Startup verify** | Before Vulkan init, `Util_VerifyManifest` checks the manifest; `Config/engine.json` `"assetVerify": "strict"` (missing → exit) or `"warn"` (missing → `[STARTUP] WARN`, continue) |

**Path examples:**

- Models / textures: `Data/Models/…`, `Data/Textures/…`
- Shader SPIR-V: `VulkanDesktop/Shader_Generated/TriangleVert.spv` (compile GLSL first)

---

## 2. Top-level shape (recommended key order)

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

Key order inside the JSON object does not affect parsing; the order above matches `demo.json` for readability.

---

## 3. Sections

### 3.1 `version` (required)

- **Type:** unsigned integer  
- **Supported today:** `1` only  
- Any other value → `[SCENE] Unsupported scene version …` at startup

### 3.2 `name` (optional)

- **Type:** string  
- Used for logs only (`[SCENE] Parsed scene v1 name='…'`)

### 3.3 `shaders` (runtime requires `lit`)

Object: keys = shader logical ids, values = SPIR-V path pairs.

```json
"shaders": {
  "lit": {
    "vert": "VulkanDesktop/Shader_Generated/TriangleVert.spv",
    "frag": "VulkanDesktop/Shader_Generated/TrianglePix.spv"
  }
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `vert` | yes | Vertex shader, repo-relative path |
| `frag` | yes | Fragment shader, repo-relative path |

**Convention:** `Vk_Core::InitVulkan` currently hard-codes the **`lit`** pair for the main graphics pipeline. Every `materials[].shader` should reference a key declared here (usually `"lit"`).

### 3.4 `logicalMeshes` (practically required when you have entities)

Defines **logical meshes** (SoA `logicalMeshId`) and **LOD chains** (lists of physical mesh ids).

```json
{
  "id": "tree",
  "lodMeshes": ["kenney_tree_detailed", "kenney_tree_simple"],
  "lodDistances": [14.0]
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `id` | yes | Logical name; referenced by `entities[].logicalMesh` |
| `lodMeshes` | yes | String array of `meshes[].id`; index 0 = nearest / highest detail |
| `lodDistances` | no | Float array; `lodDistances[i]` is world-space distance (meters) to switch from LOD i to i+1 |

**Rules:**

- `lodDistances` length must be **less than** `lodMeshes` length (N meshes → at most N−1 thresholds).  
- Single mesh: omit `lodDistances` (always LOD 0).  
- Every id in `lodMeshes` must exist in `meshes`.  
- **Logical id → runtime integer index** = **array index** in `logicalMeshes` (0-based), independent of the string id.

**LOD behavior:** see [`Data/LOD.md`](../Data/LOD.md) (includes 15% hysteresis).

### 3.5 `meshes` (required for visible geometry)

```json
{ "id": "viking_room", "path": "Data/Models/viking_room.obj" }
```

| Field | Required | Description |
|-------|----------|-------------|
| `id` | yes | Physical mesh name; referenced from `logicalMeshes[].lodMeshes` |
| `path` | yes | OBJ path (repo-relative) |

**Runtime id:** **Array index** in `meshes` (0, 1, 2, …) is the resource-table mesh id.  
If you keep demo-compatible ordering, preserve `demo.json` order or update all `logicalMeshes` references.

### 3.6 `textures`

```json
{ "id": "viking_albedo", "path": "Data/Textures/viking_room.png" }
```

| Field | Required | Description |
|-------|----------|-------------|
| `id` | yes | Referenced by `materials[].texture` |
| `path` | yes | Image path (png, jpg, …) |

**Runtime id:** array index = texture table id.

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

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `id` | yes | — | Referenced by `entities[].material` |
| `shader` | yes | — | Must exist in `shaders` |
| `texture` | yes | — | Must exist in `textures` |
| `alpha` | no | `1.0` | Material alpha (transparent materials often &lt; 1) |
| `transparent` | no | `false` | `true` → transparent pass |

**Runtime id:** array index = material table id.

### 3.8 `entities` (what to draw and where)

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

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `logicalMesh` | yes | — | Must match `logicalMeshes[].id` |
| `material` | yes | — | Must match `materials[].id` |
| `transform` | yes | — | **16 floats**, see below |
| `renderFlags` | no | `"opaque"` | `"opaque"` or `"transparent"` (unsigned integer also accepted) |
| `layerMask` | no | all bits set | Culling layer mask |
| `lodBias` | no | `0` | Added to LOD distance (meters) |

**Hierarchy:** v1 has **no** parent/child; each entity has one **world** matrix (flat scene).

---

## 4. `transform` matrix convention (important)

- **16 numbers**, **column-major**, matching **GLM** `glm::mat4` / `glm::value_ptr`.  
- Storage order: `[col0.x, col0.y, col0.z, col0.w, col1.x, …, col3.w]`.

**Translation only** (world position (-4, 0, 0)):

```json
"transform": [1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  -4, 0, 0, 1]
```

**Translation + uniform scale** (scale then translate, same as `glm::translate * glm::scale`):

```text
T * S  →  diagonal holds scale; first three components of column 3 hold translation
```

**From DCC / scripts:**

- Watch column-major vs row-major when exporting from Blender or similar.  
- Precompute with Python + GLM or engine-side helpers before writing JSON.

**Runtime:** when `ENABLE_ROTATE` is true, the demo still applies Z spin on top of the JSON **base transform** (`ApplyDemoTransformAnimation`).

---

## 5. Reference integrity checklist

After editing a scene file:

1. **`version`: 1**  
2. **`shaders.lit`** present and SPIR-V built  
3. Every id in **`logicalMeshes[].lodMeshes`** exists in **`meshes[].id`**  
4. Every **`entities[].logicalMesh`** exists in **`logicalMeshes[].id`**  
5. Every **`entities[].material`** exists in **`materials[].id`**  
6. Every **`materials[].texture`** exists in **`textures[].id`**; **`shader`** exists in **`shaders`**  
7. **`lodDistances.length` < `lodMeshes.length`** when distances are used  
8. No duplicate **`id`** within each section  
9. Run from repo root or `x64\Debug` with `--asset-root`; confirm `[STARTUP] OK` and `[SCENE] Parsed … entities=N`

---

## 6. Runtime numeric ids (`demo` scene)

Tables reflect **`demo.json` array order today**; reordering entries changes indices.

**`logicalMeshes` index → id**

| Index | id |
|-------|-----|
| 0 | viking |
| 1 | monkey |
| 2 | tree |
| 3 | rock |
| 4 | campfire |
| 5 | tent |
| 6 | stump |

**`meshes` index → id** (resource table physical mesh id)

| Index | id |
|-------|-----|
| 0 | viking_room |
| 1 | monkey |
| 2 | kenney_tree_detailed |
| 3 | kenney_tree_simple |
| 4 | kenney_rock_large |
| 5 | kenney_campfire |
| 6 | kenney_tent |
| 7 | kenney_stump |

**`materials` index → id**

| Index | id |
|-------|-----|
| 0 | mat_viking |
| 1 | mat_monkey |
| 2 | mat_transparent |
| 3 | mat_rock |
| 4 | mat_grass |
| 5 | mat_metal |
| 6 | mat_wood |

---

## 7. Minimal workflow for a new scene

1. Copy `Data/Scenes/demo.json` → `Data/Scenes/my_scene.json`.  
2. Change `name`; trim or extend `entities` / resource tables as needed.  
3. New mesh: add to `meshes`, then reference ids in `logicalMeshes.lodMeshes`.  
4. New entity: set `logicalMesh`, `material`, `transform`.  
5. Rebuild shaders if GLSL changed.  
6. Run: `VulkanDesktop.exe --scene Data/Scenes/my_scene.json`  
7. Check logs: `[STARTUP]`, `[SCENE]`, `[RESOURCE-TABLE]`, `[EXTRACT] entities=… draws=…`.

---

## 8. Common errors

| Symptom | Likely cause |
|---------|----------------|
| `[SCENE] Unsupported scene version` | `version` ≠ 1 |
| `[SCENE] Missing transform` | Entity missing `transform` or not exactly 16 floats |
| `[SCENE] Unknown logicalMesh` | `entities.logicalMesh` not defined in `logicalMeshes` |
| `[SCENE] Material '…' references unknown texture` | Wrong texture id on material |
| `[SCENE] logicalMeshes[].lodMeshes` errors | Missing `lodMeshes` or unknown mesh id |
| `[STARTUP] Missing required file` | Wrong `path` or asset root |
| Empty frame / draws=0 | No `entities`, or culling / layer mask |

---

## 9. Planned capabilities (not in v1 JSON)

| Capability | v1 JSON | Planned |
|------------|---------|---------|
| Scene unload / hot reload | **yes** — ImGui Scene panel; boot `--scene` | Phase D done |
| Warn on missing optional assets | **at startup** — `assetVerify: warn` in `engine.json` | Phase D2 |
| Parent/child hierarchy | no | scene-load non-goals; later |
| Multiple shaders / permutations | declare only; runtime uses `lit` | S2 shader stack |
| Cameras / multi-view | not in JSON | S2 multi-view |

---

## 10. Code index

| Purpose | File |
|---------|------|
| Parse | `Gfx_SceneLoader.cpp` |
| Types | `Gfx_SceneDesc.h` |
| Manifest / SoA / LOD | `Gfx_SceneApply.cpp` |
| Startup verify | `Util_AssetManifest.cpp` → `Util_VerifyManifest` |
| Entry | `VulkanDesktop.cpp` |
| GPU load | `Vk_Core.cpp` → `InitVulkan` → `LoadFromManifest` |
