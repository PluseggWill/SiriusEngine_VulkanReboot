#pragma once

#include "Util_AssetManifest.h"
#include "Util_Logger.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Central engine config: Config/engine.json + CLI overrides (CLI wins).
namespace UtilEngineConfig {

struct FeatureFlags {
    bool myDemoRotate    = true;
    bool myRuntimeMipmap = false;
};

// Parse argv for --help / unknown flags only (no logger). Returns true if process should exit (help printed).
bool TryEarlyExitFromCli( int aArgc, char** aArgv );

// Load config file + CLI; must run once before UtilLogger::Init in Application::InitApp.
void Initialize( int aArgc, char** aArgv );

bool IsInitialized();

std::filesystem::path GetAssetRoot();
std::string           GetConfigPathUsed();
std::string           GetSceneLogicalPath();
std::string           GetLogFilePath();  // Empty => default Logs/engine_runtime_log.txt under repo.

uint32_t GetWindowWidth();
uint32_t GetWindowHeight();
bool     GetVsync();

UtilLogger::LogLevel   GetMinLogLevel();
const FeatureFlags&    GetFeatures();
Util_AssetVerifyPolicy GetAssetVerifyPolicy();

// When > 0, main loop requests window close after this many rendered frames (dev smoke / CI).
int GetSmokeFrameLimit();

// When > 0, main loop also requires this many seconds after scene load (RunMainLoop start) before smoke exit.
// If both frame limit and seconds are set, both thresholds must be met.
double GetSmokeSeconds();

// Dev (S2 shader-layout 2b M4): intentional descriptor type mismatch; requires validation layers.
bool GetDescriptorLayoutMismatchTest();

// Active entry in Shader/PermutationRegistry.json (default "lit").
const std::string& GetShaderPermutationName();

// Resolved render preset label (empty when CLI/config only set shaderPermutation).
const std::string& GetRenderPresetName();

bool                              ResolveValidationEnabled( bool aBuildDefault );
bool                              IsValidationEnabled();
const std::vector< const char* >& GetValidationLayerNames();

void LogResolvedSummary();
void PrintUsage( const char* aProgramName );

}  // namespace UtilEngineConfig
