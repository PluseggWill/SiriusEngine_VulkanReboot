# Vulkan validation layers (Windows)

The engine can enable the Khronos validation layer (`VK_LAYER_KHRONOS_validation`) at instance and device creation. When validation is on, `VK_EXT_debug_utils` routes layer messages to `Logs/engine_runtime_log.txt` under `[VULKAN-VALIDATION]` (VERBOSE through ERROR).

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

## Descriptor layout mismatch test (S2 / 2b M4)

With validation installed and active:

```powershell
Set-Location x64\Debug
.\VulkanDesktop.exe --validation --descriptor-layout-mismatch-test --smoke-frames 2
```

Expect `[DESCRIPTOR] layout mismatch test OK` and a `[VULKAN-VALIDATION]` line containing `VUID` / `descriptor`. Without the Khronos layer, the process exits with an explicit error (layers requested but unavailable).

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
   - `[VULKAN] Debug utils messenger created.` when messenger setup succeeds
   - `[VULKAN-VALIDATION]` lines when the layer reports issues (may be absent on a clean run)

4. Force off without recompiling:

   ```powershell
   .\VulkanDesktop.exe --no-validation
   ```

If validation is requested but the layer is missing, the app logs a warning and continues **without** layers (fail-soft).

## Agent smoke with validation (G0-validation)

Command, when-required, and pass criteria: [`.cursor/rules/vulkan-smoke-test.mdc`](../.cursor/rules/vulkan-smoke-test.mdc) (G0-validation section). Pitfalls: `vulkan-render-pass-pitfalls.mdc`.

## Troubleshooting

| Symptom | Check |
|---------|--------|
| Layer not in discovery list | Reinstall Vulkan SDK; run `vulkaninfo` |
| `validation=disabled` in Release | Expected; use Debug or `--validation` |
| No validation messages in log | No layer violations on that run; confirm `Debug utils messenger created` and validation enabled |
| `Extension not available: VK_EXT_debug_utils` | Update GPU driver / Vulkan loader; layers may still print to the debugger only |

See also: `Docs/Archived/plans/validation-layers_Plan.md`, `Docs/Archived-Plan.md` (S0).
