# Plan: s2-init-hygiene

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Closed (2026-05-29)  
**Active-Plan:** Remove temp init hacks — Vk_Core delegation cleanup, scene bootstrap for env/camera, delete demo legacy builders.

## Problem

Post `vk-core-decomposition`, `Vk_Core` still exposes many one-line `Create*` forwards; env/camera are hardcoded in `LoadSceneResources`; `Gfx_BuildDemo*` / `Util_DemoAssets` remain as unused reference code.

## Goals

1. Remove `Vk_Core` public init methods that only delegate to peeled subsystems.
2. Move `CreateCamera` + `InitDefaultEnvironmentData` into `Vk_SceneHost::InitScenePresentation`.
3. Delete `Gfx_BuildDemoResourceManifest`, `Gfx_BuildDemoLodTable`, and `Util_DemoAssets.h`.
4. Trim obsolete peel TODOs and `USE_MANUAL_VERTICES`.
5. Update Active-Plan line wording.

## Non-goals

- Scene JSON `environment` / `cameras[]` schema.
- Shader reflection, descriptor policy changes.

## Steps

- [x] P1 — Delete `Gfx_BuildDemo*`, `Util_DemoAssets`, fix vcxproj
- [x] P2 — Remove dead `Vk_Core` forward-only init/record wrappers
- [x] P3 — `Vk_SceneHost::InitScenePresentation`
- [x] P4 — Comments; Active-Plan wording
- [x] P5 — Build + smoke (`--no-validation --smoke-seconds 6`)
- [x] P6 — Closeout archive

## Build / smoke-run

`.\VulkanDesktop.exe --no-validation --smoke-seconds 6` exit 0.
