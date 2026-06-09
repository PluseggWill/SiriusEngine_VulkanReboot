#Requires -Version 5.1
<#
.SYNOPSIS
  Verify (or apply) clang-format on VulkanDesktop C++ sources using repo-root .clang-format.

.DESCRIPTION
  Scope: VulkanDesktop/**/*.cpp and **/*.h (engine-owned sources only; excludes lib/).

  Resolves clang-format from:
  1) -ClangFormatPath / $env:CLANG_FORMAT
  2) PATH
  3) LLVM install under Program Files

  Optional -BootstrapTools downloads a portable clang-format.exe to .tools/clang-format/ (gitignored).

.EXAMPLE
  pwsh -File Scripts/Assert-ClangFormat.ps1

.EXAMPLE
  pwsh -File Scripts/Assert-ClangFormat.ps1 -Fix -BootstrapTools
#>
[CmdletBinding()]
param(
    [string] $RepoRoot = "",
    [string] $ClangFormatPath = "",
    [switch] $Fix,
    [switch] $BootstrapTools
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    param([string] $Root)
    if ([string]::IsNullOrWhiteSpace($Root)) {
        $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    } else {
        $Root = (Resolve-Path $Root).Path
    }
    if (-not (Test-Path (Join-Path $Root "VulkanDesktop.sln"))) {
        throw "VulkanDesktop.sln not found under: $Root"
    }
    return $Root
}

function Get-EngineCppSources {
    param([string] $Root)
    $sourceRoot = Join-Path $Root "VulkanDesktop"
    if (-not (Test-Path $sourceRoot)) {
        throw "VulkanDesktop/ not found under: $Root"
    }
    return @(Get-ChildItem -Path $sourceRoot -Recurse -File -Include *.cpp, *.h |
        Where-Object { $_.FullName -notmatch '\\ThirdParty\\' })
}

function Resolve-ClangFormatExe {
    param(
        [string] $Root,
        [string] $ExplicitPath,
        [switch] $AllowBootstrap
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $candidates += $ExplicitPath
    }
    if (-not [string]::IsNullOrWhiteSpace($env:CLANG_FORMAT)) {
        $candidates += $env:CLANG_FORMAT
    }
    $candidates += @(
        (Join-Path $Root ".tools\clang-format\clang-format.exe"),
        (Join-Path ${env:ProgramFiles} "LLVM\bin\clang-format.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "LLVM\bin\clang-format.exe")
    )

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $fromPath = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    if ($AllowBootstrap) {
        return Install-BootstrapClangFormat -Root $Root
    }

    throw @"
clang-format not found.

Install one of:
  winget install --id LLVM.LLVM -e
  choco install llvm -y

Or run with -BootstrapTools to download a portable binary to .tools/clang-format/ (gitignored):
  pwsh -File Scripts/Assert-ClangFormat.ps1 -Fix -BootstrapTools
"@
}

function Install-BootstrapClangFormat {
    param([string] $Root)

    $toolsDir = Join-Path $Root ".tools\clang-format"
    $exePath  = Join-Path $toolsDir "clang-format.exe"
    if (Test-Path $exePath) {
        return (Resolve-Path $exePath).Path
    }

    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null

    # Portable win64 binary (clang-format only); pinned release for reproducible CI/dev bootstrap.
    $releaseTag = "master-796e77c"
    $exeUrl     = "https://github.com/muttleyxd/clang-tools-static-binaries/releases/download/$releaseTag/clang-format-18_windows-amd64.exe"

    Write-Host "Downloading portable clang-format to $toolsDir ..."
    Invoke-WebRequest -Uri $exeUrl -OutFile $exePath -UseBasicParsing

    if (-not (Test-Path $exePath)) {
        throw "Bootstrap failed: clang-format.exe not found at $exePath"
    }

    Write-Host "Bootstrap OK: $exePath"
    return (Resolve-Path $exePath).Path
}

function Test-ClangFormatSupportsDryRunWerror {
    param([string] $Exe)
    & $Exe --help 2>&1 | Out-String | Select-String -SimpleMatch "--Werror" -Quiet
}

function Test-FileNeedsFormat {
    param(
        [string] $Exe,
        [System.IO.FileInfo] $File,
        [bool] $UseDryRunWerror
    )

    if ($UseDryRunWerror) {
        & $Exe --dry-run --Werror -style=file $File.FullName 2>&1 | Out-Null
        return ($LASTEXITCODE -ne 0)
    }

    $formatted = & $Exe -style=file $File.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "clang-format failed on $($File.FullName) with exit $LASTEXITCODE"
    }
    $original = [System.IO.File]::ReadAllText($File.FullName)
    return ($formatted -ne $original)
}

$RepoRoot = Resolve-RepoRoot -Root $RepoRoot
$styleFile = Join-Path $RepoRoot ".clang-format"
if (-not (Test-Path $styleFile)) {
    throw ".clang-format not found at repo root: $styleFile"
}

$clangFormat = Resolve-ClangFormatExe -Root $RepoRoot -ExplicitPath $ClangFormatPath -AllowBootstrap:$BootstrapTools
$sources     = Get-EngineCppSources -Root $RepoRoot
$useWerror   = Test-ClangFormatSupportsDryRunWerror -Exe $clangFormat

Write-Host "=== Assert-ClangFormat: $($sources.Count) files (style=file) ===" -ForegroundColor Cyan
Write-Host "Using: $clangFormat"

if ($Fix) {
    foreach ($file in $sources) {
        & $clangFormat -i -style=file $file.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "clang-format -i failed on $($file.FullName) with exit $LASTEXITCODE"
        }
    }
    Write-Host "=== Assert-ClangFormat: applied -i to $($sources.Count) files ===" -ForegroundColor Green
    exit 0
}

$offenders = New-Object System.Collections.Generic.List[string]
foreach ($file in $sources) {
    if (Test-FileNeedsFormat -Exe $clangFormat -File $file -UseDryRunWerror:$useWerror) {
        $offenders.Add($file.FullName.Substring($RepoRoot.Length + 1))
    }
}

if ($offenders.Count -gt 0) {
    Write-Host "clang-format drift ($($offenders.Count) file(s)):" -ForegroundColor Red
    foreach ($path in $offenders) {
        Write-Host "  $path"
    }
    Write-Host ""
    Write-Host "Fix locally: pwsh -File Scripts/Assert-ClangFormat.ps1 -Fix -BootstrapTools" -ForegroundColor Yellow
    exit 1
}

Write-Host "=== Assert-ClangFormat: PASSED ===" -ForegroundColor Green
exit 0
