#pragma once

#include "Util_AssetManifest.h"
#include "Util_Logger.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Instance-based engine config: Config/engine.json + CLI overrides (CLI wins).
// Owned by Application; pass const Util_EngineConfig& (or BindEngineConfig on Vk_Core).

struct Util_EngineConfig {
    struct FeatureFlags {
        bool myDemoRotate    = true;
        bool myRuntimeMipmap = false;
    };

    // Load config file + CLI; throws std::runtime_error on invalid input.
    void LoadFromArgv( int aArgc, char** aArgv );

    std::filesystem::path        GetAssetRoot() const;
    std::string                  GetConfigPathUsed() const;
    std::string                  GetSceneLogicalPath() const;
    std::string                  GetLogFilePath() const;
    std::string                  GetPerfLogPath() const;
    uint32_t                     GetWindowWidth() const;
    uint32_t                     GetWindowHeight() const;
    bool                         GetVsync() const;
    UtilLogger::LogLevel         GetMinLogLevel() const;
    const FeatureFlags&          GetFeatures() const;
    Util_AssetVerifyPolicy       GetAssetVerifyPolicy() const;
    int                          GetSmokeFrameLimit() const;
    double                       GetSmokeSeconds() const;
    bool                         GetDescriptorLayoutMismatchTest() const;
    bool                         GetEnableRenderDoc() const;
    bool                         GetLegacyDirectDraw() const;
    const std::string&           GetShaderPermutationName() const;
    const std::string&           GetRenderPresetName() const;
    bool                         ResolveValidationEnabled( bool aBuildDefault ) const;
    bool                         IsValidationEnabled() const;
    const std::vector< const char* >& GetValidationLayerNames() const;
    void LogResolvedSummary() const;

private:
    struct CliOverrides {
        std::optional< std::string >           myAssetRoot;
        std::optional< std::filesystem::path > myConfigPath;
        std::optional< std::string >           myScene;
        std::optional< std::string >           myLogLevel;
        std::optional< bool >                  myVsync;
        std::optional< uint32_t >              myWindowWidth;
        std::optional< uint32_t >              myWindowHeight;
        std::optional< bool >                  myDemoRotate;
        std::optional< bool >                  myLegacyDirectDraw;
        std::optional< bool >                  myRuntimeMipmap;
        std::optional< int >                   mySmokeFrames;
        std::optional< double >                mySmokeSeconds;
        std::optional< std::string >           myShaderPermutation;
        std::optional< std::string >           myRenderPreset;
        std::optional< std::string >           myPerfLog;
    };

    CliOverrides ParseCliOverrides( int aArgc, char** aArgv );
    void         ApplyJsonFile( const std::filesystem::path& aConfigPath );
    void         ApplyCliOverrides( const CliOverrides& aOverrides );
    void         ResolveActiveShaderPermutation( const CliOverrides& aOverrides );

    std::filesystem::path myAssetRoot;
    std::string                    myConfigPathUsed;
    std::string                    mySceneLogicalPath;
    std::string                    myLogFilePath;
    std::string                    myPerfLogPath;
    uint32_t                       myWindowWidth  = 1600;
    uint32_t                       myWindowHeight   = 1200;
    bool                           myVsync         = true;
    UtilLogger::LogLevel           myMinLogLevel    = UtilLogger::LogLevel::Info;
    FeatureFlags                   myFeatures{};
    Util_AssetVerifyPolicy         myAssetVerifyPolicy            = Util_AssetVerifyPolicy::Strict;
    int                            mySmokeFrameLimit              = 0;
    double                         mySmokeSeconds                 = 0.0;
    bool                           myDescriptorLayoutMismatchTest = false;
    bool                           myEnableRenderDoc              = false;
    bool                           myLegacyDirectDraw             = false;  // --legacy-direct-draw: vkCmdDrawIndexed fallback (M2 prep debug)
    std::string                    myShaderPermutationName        = "lit";
    std::string                    myRenderPresetName;
    std::optional< std::string >   myConfigShaderPermutation;
    std::optional< std::string >   myConfigRenderPreset;
    std::optional< bool >          myCliValidationOverride;
    std::optional< bool >          myConfigValidation;
    mutable bool                   myValidationResolved = false;
    mutable bool                   myValidationEnabled  = false;
    std::vector< const char* >     myValidationLayers{ "VK_LAYER_KHRONOS_validation" };
};

// Early CLI (--help) without constructing config.
namespace UtilEngineConfig {
bool TryEarlyExitFromCli( int aArgc, char** aArgv );
void PrintUsage( const char* aProgramName );
}  // namespace UtilEngineConfig
