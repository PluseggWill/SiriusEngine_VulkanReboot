#include "Util_EngineConfig.h"

#include "../Gfx/Gfx_SceneDesc.h"
#include "Util_EngineConfigInternal.h"

#include <stdexcept>

using UtilEngineConfigInternal::DefaultConfigPath;
using UtilEngineConfigInternal::FindRepoRoot;
using UtilEngineConfigInternal::LogLevelName;
using UtilEngineConfigInternal::RequireDirectory;
using UtilEngineConfigInternal::ResolvePathArgument;

void Util_EngineConfig::LoadFromArgv( int aArgc, char** aArgv ) {
    mySceneLogicalPath = kGfxDefaultSceneLogicalPath;
    myAssetRoot        = FindRepoRoot();

    CliOverrides overrides = ParseCliOverrides( aArgc, aArgv );

    const std::filesystem::path configPath = overrides.myConfigPath ? ResolvePathArgument( overrides.myConfigPath->string() ) : DefaultConfigPath();
    myConfigPathUsed                       = configPath.string();

    if ( std::filesystem::exists( configPath ) ) {
        ApplyJsonFile( configPath );
    }
    else if ( overrides.myConfigPath.has_value() ) {
        throw std::runtime_error( "Config file not found: " + myConfigPathUsed );
    }

    ApplyCliOverrides( overrides );

    if ( !overrides.myAssetRoot.has_value() && myAssetRoot.empty() ) {
        myAssetRoot = FindRepoRoot();
    }

    RequireDirectory( myAssetRoot, "asset root" );

    if ( myWindowWidth == 0 || myWindowHeight == 0 ) {
        throw std::runtime_error( "window width/height must be > 0" );
    }
}

std::filesystem::path Util_EngineConfig::GetAssetRoot() const {
    return myAssetRoot;
}

std::string Util_EngineConfig::GetConfigPathUsed() const {
    return myConfigPathUsed;
}

std::string Util_EngineConfig::GetSceneLogicalPath() const {
    return mySceneLogicalPath;
}

std::string Util_EngineConfig::GetLogFilePath() const {
    return myLogFilePath;
}

std::string Util_EngineConfig::GetPerfLogPath() const {
    return myPerfLogPath;
}

uint32_t Util_EngineConfig::GetWindowWidth() const {
    return myWindowWidth;
}

uint32_t Util_EngineConfig::GetWindowHeight() const {
    return myWindowHeight;
}

bool Util_EngineConfig::GetVsync() const {
    return myVsync;
}

UtilLogger::LogLevel Util_EngineConfig::GetMinLogLevel() const {
    return myMinLogLevel;
}

const Util_EngineConfig::FeatureFlags& Util_EngineConfig::GetFeatures() const {
    return myFeatures;
}

Util_AssetVerifyPolicy Util_EngineConfig::GetAssetVerifyPolicy() const {
    return myAssetVerifyPolicy;
}

int Util_EngineConfig::GetSmokeFrameLimit() const {
    return mySmokeFrameLimit;
}

double Util_EngineConfig::GetSmokeSeconds() const {
    return mySmokeSeconds;
}

bool Util_EngineConfig::GetDescriptorLayoutMismatchTest() const {
    return myDescriptorLayoutMismatchTest;
}

bool Util_EngineConfig::GetEnableRenderDoc() const {
    return myEnableRenderDoc;
}

bool Util_EngineConfig::GetLegacyDirectDraw() const {
    return myLegacyDirectDraw;
}

bool Util_EngineConfig::GetGpuCullEnabled() const {
    return myGpuCullEnabled;
}

bool Util_EngineConfig::ResolveValidationEnabled( bool aBuildDefault ) const {
    if ( myCliValidationOverride.has_value() ) {
        myValidationEnabled = *myCliValidationOverride;
    }
    else if ( myConfigValidation.has_value() ) {
        myValidationEnabled = *myConfigValidation;
    }
    else {
        myValidationEnabled = aBuildDefault;
    }

    myValidationResolved = true;
    return myValidationEnabled;
}

bool Util_EngineConfig::IsValidationEnabled() const {
    if ( !myValidationResolved ) {
        return ResolveValidationEnabled( false );
    }
    return myValidationEnabled;
}

const std::vector< const char* >& Util_EngineConfig::GetValidationLayerNames() const {
    return myValidationLayers;
}

const std::string& Util_EngineConfig::GetShaderPermutationName() const {
    return myShaderPermutationName;
}

const std::string& Util_EngineConfig::GetRenderPresetName() const {
    return myRenderPresetName;
}

const Gfx_LightingSettings& Util_EngineConfig::GetLightingSettings() const {
    return myLightingSettings;
}

const std::string& Util_EngineConfig::GetEnvironmentPath() const {
    return myEnvironmentPath;
}

void Util_EngineConfig::LogResolvedSummary() const {
    UtilLogger::Info( "CONFIG", "cwd=" + std::filesystem::current_path().string() );
    UtilLogger::Info( "CONFIG", "config=" + myConfigPathUsed );
    UtilLogger::Info( "CONFIG", "assetRoot=" + std::filesystem::weakly_canonical( myAssetRoot ).string() );
    UtilLogger::Info( "CONFIG", "scene=" + mySceneLogicalPath );
    UtilLogger::Info( "CONFIG", "window=" + std::to_string( myWindowWidth ) + "x" + std::to_string( myWindowHeight ) + " vsync=" + ( myVsync ? "on" : "off" ) );
    UtilLogger::Info( "CONFIG", std::string( "logLevel=" ) + LogLevelName( myMinLogLevel ) );
    UtilLogger::Info( "CONFIG", std::string( "features demoRotate=" ) + ( myFeatures.myDemoRotate ? "true" : "false" ) + " lodEnabled="
                                    + ( myFeatures.myLodEnabled ? "true" : "false" ) + " runtimeMipmap=" + ( myFeatures.myRuntimeMipmap ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "assetVerify=" ) + ( myAssetVerifyPolicy == Util_AssetVerifyPolicy::Strict ? "strict" : "warn" ) );
    if ( !myRenderPresetName.empty() ) {
        UtilLogger::Info( "CONFIG", "renderPreset=" + myRenderPresetName );
    }
    UtilLogger::Info( "CONFIG", "shaderPermutation=" + myShaderPermutationName );
    UtilLogger::Info( "CONFIG", std::string( "renderdoc=" ) + ( myEnableRenderDoc ? "enabled" : "disabled" ) );
    UtilLogger::Info( "CONFIG", std::string( "legacyDirectDraw=" ) + ( myLegacyDirectDraw ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "gpuCull=" ) + ( myGpuCullEnabled ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "lighting shadows=" ) + ( myLightingSettings.myShadowsEnabled ? "1" : "0" )
                                    + " ibl=" + ( myLightingSettings.myIblEnabled ? "1" : "0" ) + " iblIntensity=" + std::to_string( myLightingSettings.myIblIntensity )
                                    + " ddgi=" + ( myLightingSettings.myDdgiEnabled ? "1" : "0" ) + " ddgiBlend=" + std::to_string( myLightingSettings.myDdgiHistoryBlend )
                                    + " environment=" + myEnvironmentPath );

    if ( myValidationResolved ) {
        const char* source = "build default";
        if ( myCliValidationOverride.has_value() ) {
            source = "CLI";
        }
        else if ( myConfigValidation.has_value() ) {
            source = "config";
        }
        UtilLogger::Info( "CONFIG", std::string( "validationLayers=" ) + ( myValidationEnabled ? "enabled" : "disabled" ) + " (" + source + ")" );
    }
}
