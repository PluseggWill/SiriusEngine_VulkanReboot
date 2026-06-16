param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$Scene = "Data/Scenes/sponza.json",
    [int]$SmokeFrames = 240,
    [int]$SmokeSeconds = 8
)

$exe = Join-Path $RepoRoot "x64\Debug\VulkanDesktop.exe"
if (-not (Test-Path $exe)) {
    throw "Missing executable: $exe (run Verify-CI.ps1 first)."
}

$perfOff = Join-Path $RepoRoot "Logs\perf_ddgi_off.jsonl"
$perfOn = Join-Path $RepoRoot "Logs\perf_ddgi_on.jsonl"
if (Test-Path $perfOff) { Remove-Item $perfOff -Force }
if (Test-Path $perfOn) { Remove-Item $perfOn -Force }

Write-Host "=== DDGI benchmark: OFF ==="
& $exe --asset-root $RepoRoot --scene $Scene --smoke-frames $SmokeFrames --smoke-seconds $SmokeSeconds --no-validation --no-ddgi --perf-log $perfOff
if ($LASTEXITCODE -ne 0) { throw "DDGI off run failed: exit $LASTEXITCODE" }

Write-Host "=== DDGI benchmark: ON ==="
& $exe --asset-root $RepoRoot --scene $Scene --smoke-frames $SmokeFrames --smoke-seconds $SmokeSeconds --no-validation --ddgi --perf-log $perfOn
if ($LASTEXITCODE -ne 0) { throw "DDGI on run failed: exit $LASTEXITCODE" }

Write-Host "=== DDGI benchmark summary ==="
Write-Host "off: $perfOff"
Write-Host "on : $perfOn"
Write-Host "Tip: powershell -File Scripts/Perf-JsonlSummary.ps1 -InputPath $perfOff"
Write-Host "Tip: powershell -File Scripts/Perf-JsonlSummary.ps1 -InputPath $perfOn"
