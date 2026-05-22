# Vulkan validation layers (Windows)

The engine can enable the Khronos validation layer (`VK_LAYER_KHRONOS_validation`) at instance and device creation. Messages appear in the debugger output and (with a future debug messenger) in the log.

## Install

1. Install the [Vulkan SDK](https://vulkan.lunarg.com/) (1.2+; this repo vendors 1.2.182 under `lib/VulkanSDK/` for builds).
2. Run the SDK installer and include **Vulkan SDK Core** and **Shader Toolchain**.
3. Confirm the layer is visible:

   ```powershell
   & "$env:VULKAN_SDK\Bin\vulkaninfo.exe" | Select-String "VK_LAYER_KHRONOS_validation"
   ```

   If `VULKAN_SDK` is unset, use the SDK `Bin` folder from your install path (e.g. `C:\VulkanSDK\1.3.xxx\Bin\vulkaninfo.exe`).

4. Optional: open **Vulkan Config** (`vkconfig`) from the Start Menu and ensure the Khronos Validation layer is enabled for your Vulkan installation.

## Runtime control

| Source | Effect |
|--------|--------|
| **Debug build** (`Debug\|x64`) | Validation **on** by default |
| **Release build** | Validation **off** by default |
| `Config/engine.json` | `"enableValidationLayers": true` or `false` |
| `--validation` | Force **on** (overrides config) |
| `--no-validation` | Force **off** (overrides config) |

Example `Config/engine.json`:

```json
{
  "assetRoot": "",
  "enableValidationLayers": true
}
```

## Verify in this project

1. Build `VulkanDesktop.sln` (Debug\|x64).
2. Run from `x64\Debug`:

   ```powershell
   .\VulkanDesktop.exe
   ```

3. Open `Logs/engine_runtime_log.txt` and look for:

   - `[VULKAN] Instance layer discovery (N available):`
   - `[VULKAN] Validation layers: enabled` (or `disabled`)
   - `[VULKAN] Requested validation layer present: VK_LAYER_KHRONOS_validation` when enabled

4. Force off without recompiling:

   ```powershell
   .\VulkanDesktop.exe --no-validation
   ```

If validation is requested but the layer is missing, the app logs a warning and continues **without** layers (fail-soft).

## Troubleshooting

| Symptom | Check |
|---------|--------|
| Layer not in discovery list | Reinstall Vulkan SDK; run `vulkaninfo` |
| `validation=disabled` in Release | Expected; use Debug or `--validation` |
| No validation messages in log | Debug messenger not implemented yet (S0 Should); use Visual Studio **Output** window or attach debugger |

See also: `Docs/validation-layers_Plan.md`, `Docs/SprintPlan.md` (S0).
