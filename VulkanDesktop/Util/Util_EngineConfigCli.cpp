#include "Util_EngineConfig.h"

#include "../Gfx/Gfx_RenderPreset.h"
#include "Util_EngineConfigInternal.h"

#include <iostream>
#include <stdexcept>

using UtilEngineConfigInternal::ParseLogLevel;
using UtilEngineConfigInternal::ResolvePathArgument;

void Util_EngineConfig::ResolveActiveShaderPermutation( const CliOverrides& aOverrides ) {
    if ( aOverrides.myShaderPermutation.has_value() ) {
        myShaderPermutationName = *aOverrides.myShaderPermutation;
        return;
    }
    if ( aOverrides.myRenderPreset.has_value() ) {
        myRenderPresetName      = *aOverrides.myRenderPreset;
        myShaderPermutationName = Gfx_RenderPreset::ToShaderPermutationName( myRenderPresetName );
        return;
    }
    if ( myConfigShaderPermutation.has_value() ) {
        myShaderPermutationName = *myConfigShaderPermutation;
        return;
    }
    if ( myConfigRenderPreset.has_value() ) {
        myRenderPresetName      = *myConfigRenderPreset;
        myShaderPermutationName = Gfx_RenderPreset::ToShaderPermutationName( myRenderPresetName );
        return;
    }
    // Default when config omits renderPreset (HybridDeferred maps to lit permutation).
    myRenderPresetName = "HybridDeferred";
}

Util_EngineConfig::CliOverrides Util_EngineConfig::ParseCliOverrides( int aArgc, char** aArgv ) {
    CliOverrides overrides;
    for ( int i = 1; i < aArgc; ++i ) {
        const std::string arg = aArgv[ i ];
        if ( arg == "--asset-root" || arg == "--assetroot" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --asset-root" );
            }
            overrides.myAssetRoot = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--config" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --config" );
            }
            overrides.myConfigPath = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--scene" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --scene" );
            }
            overrides.myScene = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--log-level" || arg == "--loglevel" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --log-level" );
            }
            overrides.myLogLevel = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--vsync" ) {
            overrides.myVsync = true;
            continue;
        }
        if ( arg == "--no-vsync" ) {
            overrides.myVsync = false;
            continue;
        }
        if ( arg == "--width" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --width" );
            }
            overrides.myWindowWidth = static_cast< uint32_t >( std::stoul( aArgv[ ++i ] ) );
            continue;
        }
        if ( arg == "--height" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --height" );
            }
            overrides.myWindowHeight = static_cast< uint32_t >( std::stoul( aArgv[ ++i ] ) );
            continue;
        }
        if ( arg == "--demo-rotate" ) {
            overrides.myDemoRotate = true;
            continue;
        }
        if ( arg == "--no-demo-rotate" ) {
            overrides.myDemoRotate = false;
            continue;
        }
        if ( arg == "--lod-enabled" ) {
            overrides.myLodEnabled = true;
            continue;
        }
        if ( arg == "--no-lod-enabled" ) {
            overrides.myLodEnabled = false;
            continue;
        }
        if ( arg == "--legacy-direct-draw" ) {
            overrides.myLegacyDirectDraw = true;
            continue;
        }
        if ( arg == "--gpu-cull" ) {
            overrides.myGpuCullEnabled = true;
            continue;
        }
        if ( arg == "--no-gpu-cull" ) {
            overrides.myGpuCullEnabled = false;
            continue;
        }
        if ( arg == "--smoke-frames" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --smoke-frames" );
            }
            overrides.mySmokeFrames = std::stoi( aArgv[ ++i ] );
            if ( *overrides.mySmokeFrames <= 0 ) {
                throw std::runtime_error( "--smoke-frames must be > 0" );
            }
            continue;
        }
        if ( arg == "--smoke-seconds" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --smoke-seconds" );
            }
            overrides.mySmokeSeconds = std::stod( aArgv[ ++i ] );
            if ( *overrides.mySmokeSeconds <= 0.0 ) {
                throw std::runtime_error( "--smoke-seconds must be > 0" );
            }
            continue;
        }
        if ( arg == "--validation" || arg == "--enable-validation" ) {
            myCliValidationOverride = true;
            continue;
        }
        if ( arg == "--no-validation" || arg == "--disable-validation" ) {
            myCliValidationOverride = false;
            continue;
        }
        if ( arg == "--shader-permutation" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --shader-permutation" );
            }
            overrides.myShaderPermutation = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--render-preset" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --render-preset" );
            }
            overrides.myRenderPreset = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--descriptor-layout-mismatch-test" ) {
            myDescriptorLayoutMismatchTest = true;
            continue;
        }
        if ( arg == "--renderdoc" ) {
            myEnableRenderDoc = true;
            continue;
        }
        if ( arg == "--perf-log" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --perf-log" );
            }
            overrides.myPerfLog = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--shadows" ) {
            overrides.myShadowsEnabled = true;
            continue;
        }
        if ( arg == "--no-shadows" ) {
            overrides.myShadowsEnabled = false;
            continue;
        }
        if ( arg == "--ibl" ) {
            overrides.myIblEnabled = true;
            continue;
        }
        if ( arg == "--no-ibl" ) {
            overrides.myIblEnabled = false;
            continue;
        }
        if ( arg == "--ibl-intensity" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --ibl-intensity" );
            }
            overrides.myIblIntensity = static_cast< float >( std::stod( aArgv[ ++i ] ) );
            continue;
        }
        if ( arg == "--environment" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --environment" );
            }
            overrides.myEnvironment = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--ddgi" ) {
            overrides.myDdgiEnabled = true;
            continue;
        }
        if ( arg == "--no-ddgi" ) {
            overrides.myDdgiEnabled = false;
            continue;
        }
        if ( arg == "--help" || arg == "-h" || arg == "/?" ) {
            continue;
        }
        throw std::runtime_error( "Unknown argument: " + arg );
    }
    return overrides;
}

void Util_EngineConfig::ApplyCliOverrides( const CliOverrides& aOverrides ) {
    if ( aOverrides.myAssetRoot.has_value() ) {
        myAssetRoot = ResolvePathArgument( *aOverrides.myAssetRoot );
    }
    if ( aOverrides.myScene.has_value() ) {
        mySceneLogicalPath = *aOverrides.myScene;
    }
    if ( aOverrides.myLogLevel.has_value() ) {
        myMinLogLevel = ParseLogLevel( *aOverrides.myLogLevel );
    }
    if ( aOverrides.myVsync.has_value() ) {
        myVsync = *aOverrides.myVsync;
    }
    if ( aOverrides.myWindowWidth.has_value() ) {
        myWindowWidth = *aOverrides.myWindowWidth;
    }
    if ( aOverrides.myWindowHeight.has_value() ) {
        myWindowHeight = *aOverrides.myWindowHeight;
    }
    if ( aOverrides.myDemoRotate.has_value() ) {
        myFeatures.myDemoRotate = *aOverrides.myDemoRotate;
    }
    if ( aOverrides.myLodEnabled.has_value() ) {
        myFeatures.myLodEnabled = *aOverrides.myLodEnabled;
    }
    if ( aOverrides.myLegacyDirectDraw.has_value() ) {
        myLegacyDirectDraw = *aOverrides.myLegacyDirectDraw;
    }
    if ( aOverrides.myGpuCullEnabled.has_value() ) {
        myGpuCullEnabled = *aOverrides.myGpuCullEnabled;
    }
    if ( aOverrides.myRuntimeMipmap.has_value() ) {
        myFeatures.myRuntimeMipmap = *aOverrides.myRuntimeMipmap;
    }
    if ( aOverrides.mySmokeFrames.has_value() ) {
        mySmokeFrameLimit = *aOverrides.mySmokeFrames;
    }
    if ( aOverrides.mySmokeSeconds.has_value() ) {
        mySmokeSeconds = *aOverrides.mySmokeSeconds;
    }
    if ( aOverrides.myPerfLog.has_value() ) {
        myPerfLogPath = *aOverrides.myPerfLog;
    }
    if ( aOverrides.myShadowsEnabled.has_value() ) {
        myLightingSettings.myShadowsEnabled = *aOverrides.myShadowsEnabled;
    }
    if ( aOverrides.myIblEnabled.has_value() ) {
        myLightingSettings.myIblEnabled = *aOverrides.myIblEnabled;
    }
    if ( aOverrides.myIblIntensity.has_value() ) {
        myLightingSettings.myIblIntensity = *aOverrides.myIblIntensity;
    }
    if ( aOverrides.myEnvironment.has_value() ) {
        myEnvironmentPath = *aOverrides.myEnvironment;
    }
    if ( aOverrides.myDdgiEnabled.has_value() ) {
        myLightingSettings.myDdgiEnabled = *aOverrides.myDdgiEnabled;
    }
    ResolveActiveShaderPermutation( aOverrides );
}
namespace UtilEngineConfig {

bool TryEarlyExitFromCli( int aArgc, char** aArgv ) {
    for ( int i = 1; i < aArgc; ++i ) {
        const std::string arg = aArgv[ i ];
        if ( arg == "--help" || arg == "-h" || arg == "/?" ) {
            PrintUsage( aArgc > 0 ? aArgv[ 0 ] : nullptr );
            return true;
        }
    }
    return false;
}

void PrintUsage( const char* aProgramName ) {
    const char* name = ( aProgramName != nullptr && aProgramName[ 0 ] != '\0' ) ? aProgramName : "VulkanDesktop";
    std::cerr << "Usage: " << name << " [options]\n"
              << "  --config <file>        JSON config (default Config/engine.json under repo root)\n"
              << "  --asset-root <dir>     Repository / content root (contains Data/, VulkanDesktop/)\n"
              << "  --scene <path>         Scene JSON (repo-relative; default Data/Scenes/sponza.json)\n"
              << "  --width <n>            Window width (overrides config)\n"
              << "  --height <n>           Window height (overrides config)\n"
              << "  --vsync / --no-vsync   Swapchain present mode preference\n"
              << "  --log-level <level>    debug | info | warn | error\n"
              << "  --validation / --enable-validation   Enable Vulkan validation layers\n"
              << "  --no-validation / --disable-validation   Disable validation layers\n"
              << "  --demo-rotate / --no-demo-rotate   Demo Z spin on entities\n"
              << "  --lod-enabled / --no-lod-enabled   CPU mesh LOD selection in draw stream\n"
              << "  --legacy-direct-draw   Use vkCmdDrawIndexed per draw (M2 prep debug fallback)\n"
              << "  --gpu-cull / --no-gpu-cull   Enable compute frustum cull to slot indirect buffer (P3; default off)\n"
              << "  --smoke-frames <n>     Exit after n rendered frames (dev smoke / CI)\n"
              << "  --smoke-seconds <s>    Exit after s seconds in main loop (post scene load; task smoke)\n"
              << "  --perf-log <path>      Append per-frame JSONL metrics (schemaVersion 1)\n"
              << "  --shader-permutation <name>   Active entry in PermutationRegistry.json (e.g. lit, lit_alpha_clip)\n"
              << "  --render-preset <name>        Render preset (HybridDeferred, ForwardLit, ForwardLitAlphaClip); overridden by --shader-permutation\n"
              << "  --shadows / --no-shadows      Enable or disable directional shadow map (overrides config)\n"
              << "  --ibl / --no-ibl              Enable or disable split-sum IBL (overrides config)\n"
              << "  --ibl-intensity <f>           IBL intensity multiplier (overrides config)\n"
              << "  --ddgi / --no-ddgi            Enable or disable DDGI probe contribution\n"
              << "  --environment <path>          IBL cubemap manifest directory (repo-relative)\n"
              << "  --descriptor-layout-mismatch-test   Dev: vkUpdateDescriptorSets type mismatch probe (needs --validation)\n"
              << "  --renderdoc            Enable RenderDoc runtime integration (startup-gated)\n"
              << "  --help                 Show this message\n"
              << "\nFull reference: Docs/CLI.md (engine.json keys, priority, examples).\n";
}

}  // namespace UtilEngineConfig
