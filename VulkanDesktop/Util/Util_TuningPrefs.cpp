#include "Util_TuningPrefs.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../RenderCore/Vk_Renderer.h"
#include "Util_Logger.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace Util_TuningPrefs {
namespace {

    nlohmann::json Vec4ToJson( const glm::vec4& aVec ) {
        return nlohmann::json::array( { aVec.x, aVec.y, aVec.z, aVec.w } );
    }

    glm::vec4 JsonToVec4( const nlohmann::json& aJson ) {
        if ( !aJson.is_array() || aJson.size() != 4 ) {
            throw std::runtime_error( "tuning: expected vec4 array" );
        }
        return glm::vec4( aJson[ 0 ].get< float >(), aJson[ 1 ].get< float >(), aJson[ 2 ].get< float >(), aJson[ 3 ].get< float >() );
    }

    glm::vec3 JsonToVec3( const nlohmann::json& aJson ) {
        if ( !aJson.is_array() || aJson.size() != 3 ) {
            throw std::runtime_error( "tuning: expected vec3 array" );
        }
        return glm::vec3( aJson[ 0 ].get< float >(), aJson[ 1 ].get< float >(), aJson[ 2 ].get< float >() );
    }

    const char* AoMethodToString( Gfx_AoMethod aMethod ) {
        return Gfx_AoMethodLabel( aMethod );
    }

    Gfx_AoMethod AoMethodFromString( const std::string& aName ) {
        if ( aName == Gfx_AoMethodLabel( Gfx_AoMethod::ClassicSsao ) ) {
            return Gfx_AoMethod::ClassicSsao;
        }
        if ( aName == Gfx_AoMethodLabel( Gfx_AoMethod::HbaoPlus ) ) {
            return Gfx_AoMethod::HbaoPlus;
        }
        if ( aName == Gfx_AoMethodLabel( Gfx_AoMethod::Gtao ) ) {
            return Gfx_AoMethod::Gtao;
        }
        throw std::runtime_error( "tuning: unknown AO method: " + aName );
    }

    nlohmann::json LightingToJson( const Gfx_LightingSettings& aLighting ) {
        return nlohmann::json{
            { "shadowsEnabled", aLighting.myShadowsEnabled },
            { "iblEnabled", aLighting.myIblEnabled },
            { "iblIntensity", aLighting.myIblIntensity },
            { "iblSpecularShadowMin", aLighting.myIblSpecularShadowMin },
            { "specularOcclusionEnabled", aLighting.mySpecularOcclusionEnabled },
            { "ssrEnabled", aLighting.mySsrEnabled },
            { "ssrMaxRoughness", aLighting.mySsrMaxRoughness },
            { "ssrMaxDistance", aLighting.mySsrMaxDistance },
            { "ssrThickness", aLighting.mySsrThickness },
            { "ssrMaxSteps", aLighting.mySsrMaxSteps },
            { "ddgiEnabled", aLighting.myDdgiEnabled },
            { "ddgiStaggeredUpdate", aLighting.myDdgiStaggeredUpdate },
            { "ddgiIntensity", aLighting.myDdgiIntensity },
            { "ddgiDebugOverlay", aLighting.myDdgiDebugOverlay },
            { "ddgiHistoryBlend", aLighting.myDdgiHistoryBlend },
            { "ddgiProbeCountX", aLighting.myDdgiProbeCountX },
            { "ddgiProbeCountY", aLighting.myDdgiProbeCountY },
            { "ddgiProbeCountZ", aLighting.myDdgiProbeCountZ },
            { "ddgiUpdateBudget", aLighting.myDdgiUpdateBudget },
            { "ddgiVolumeCenter", nlohmann::json::array( { aLighting.myDdgiVolumeCenter.x, aLighting.myDdgiVolumeCenter.y, aLighting.myDdgiVolumeCenter.z } ) },
            { "ddgiVolumeExtents", nlohmann::json::array( { aLighting.myDdgiVolumeExtents.x, aLighting.myDdgiVolumeExtents.y, aLighting.myDdgiVolumeExtents.z } ) }
        };
    }

    void LightingFromJson( const nlohmann::json& aJson, Gfx_LightingSettings& aOut ) {
        if ( aJson.contains( "shadowsEnabled" ) ) {
            aOut.myShadowsEnabled = aJson[ "shadowsEnabled" ].get< bool >();
        }
        if ( aJson.contains( "iblEnabled" ) ) {
            aOut.myIblEnabled = aJson[ "iblEnabled" ].get< bool >();
        }
        if ( aJson.contains( "iblIntensity" ) ) {
            aOut.myIblIntensity = aJson[ "iblIntensity" ].get< float >();
        }
        if ( aJson.contains( "iblSpecularShadowMin" ) ) {
            aOut.myIblSpecularShadowMin = aJson[ "iblSpecularShadowMin" ].get< float >();
        }
        if ( aJson.contains( "specularOcclusionEnabled" ) ) {
            aOut.mySpecularOcclusionEnabled = aJson[ "specularOcclusionEnabled" ].get< bool >();
        }
        if ( aJson.contains( "ssrEnabled" ) ) {
            aOut.mySsrEnabled = aJson[ "ssrEnabled" ].get< bool >();
        }
        if ( aJson.contains( "ssrMaxRoughness" ) ) {
            aOut.mySsrMaxRoughness = aJson[ "ssrMaxRoughness" ].get< float >();
        }
        if ( aJson.contains( "ssrMaxDistance" ) ) {
            aOut.mySsrMaxDistance = aJson[ "ssrMaxDistance" ].get< float >();
        }
        if ( aJson.contains( "ssrThickness" ) ) {
            aOut.mySsrThickness = aJson[ "ssrThickness" ].get< float >();
        }
        if ( aJson.contains( "ssrMaxSteps" ) ) {
            aOut.mySsrMaxSteps = aJson[ "ssrMaxSteps" ].get< uint32_t >();
        }
        if ( aJson.contains( "ddgiEnabled" ) ) {
            aOut.myDdgiEnabled = aJson[ "ddgiEnabled" ].get< bool >();
        }
        if ( aJson.contains( "ddgiStaggeredUpdate" ) ) {
            aOut.myDdgiStaggeredUpdate = aJson[ "ddgiStaggeredUpdate" ].get< bool >();
        }
        if ( aJson.contains( "ddgiIntensity" ) ) {
            aOut.myDdgiIntensity = aJson[ "ddgiIntensity" ].get< float >();
        }
        if ( aJson.contains( "ddgiDebugOverlay" ) ) {
            aOut.myDdgiDebugOverlay = aJson[ "ddgiDebugOverlay" ].get< float >();
        }
        if ( aJson.contains( "ddgiHistoryBlend" ) ) {
            aOut.myDdgiHistoryBlend = aJson[ "ddgiHistoryBlend" ].get< float >();
        }
        if ( aJson.contains( "ddgiProbeCountX" ) ) {
            aOut.myDdgiProbeCountX = aJson[ "ddgiProbeCountX" ].get< uint32_t >();
        }
        if ( aJson.contains( "ddgiProbeCountY" ) ) {
            aOut.myDdgiProbeCountY = aJson[ "ddgiProbeCountY" ].get< uint32_t >();
        }
        if ( aJson.contains( "ddgiProbeCountZ" ) ) {
            aOut.myDdgiProbeCountZ = aJson[ "ddgiProbeCountZ" ].get< uint32_t >();
        }
        if ( aJson.contains( "ddgiUpdateBudget" ) ) {
            aOut.myDdgiUpdateBudget = aJson[ "ddgiUpdateBudget" ].get< uint32_t >();
        }
        if ( aJson.contains( "ddgiVolumeCenter" ) ) {
            aOut.myDdgiVolumeCenter = JsonToVec3( aJson[ "ddgiVolumeCenter" ] );
        }
        if ( aJson.contains( "ddgiVolumeExtents" ) ) {
            aOut.myDdgiVolumeExtents = JsonToVec3( aJson[ "ddgiVolumeExtents" ] );
        }
    }

    nlohmann::json AoToJson( const Gfx_AoSettings& aAo ) {
        return nlohmann::json{ { "method", AoMethodToString( aAo.myMethod ) },
                               { "enabled", aAo.myEnabled },
                               { "radius", aAo.myRadius },
                               { "bias", aAo.myBias },
                               { "intensity", aAo.myIntensity },
                               { "power", aAo.myPower },
                               { "hbaoDirections", aAo.myHbaoDirections },
                               { "hbaoSteps", aAo.myHbaoSteps },
                               { "gtaoSlices", aAo.myGtaoSlices },
                               { "gtaoStepsPerSlice", aAo.myGtaoStepsPerSlice },
                               { "gtaoFalloff", aAo.myGtaoFalloff },
                               { "upsampleDepthSigma", aAo.myUpsampleDepthSigma },
                               { "normalAwareRadius", aAo.myNormalAwareRadius },
                               { "temporalEnabled", aAo.myTemporalEnabled },
                               { "temporalBlend", aAo.myTemporalBlend },
                               { "contactSoftEnabled", aAo.myContactSoftEnabled },
                               { "contactSoftBlurRadius", aAo.myContactSoftBlurRadius },
                               { "contactSoftDepthSigma", aAo.myContactSoftDepthSigma } };
    }

    void AoFromJson( const nlohmann::json& aJson, Gfx_AoSettings& aOut ) {
        if ( aJson.contains( "method" ) ) {
            aOut.myMethod = AoMethodFromString( aJson[ "method" ].get< std::string >() );
        }
        if ( aJson.contains( "enabled" ) ) {
            aOut.myEnabled = aJson[ "enabled" ].get< bool >();
        }
        if ( aJson.contains( "radius" ) ) {
            aOut.myRadius = aJson[ "radius" ].get< float >();
        }
        if ( aJson.contains( "bias" ) ) {
            aOut.myBias = aJson[ "bias" ].get< float >();
        }
        if ( aJson.contains( "intensity" ) ) {
            aOut.myIntensity = aJson[ "intensity" ].get< float >();
        }
        if ( aJson.contains( "power" ) ) {
            aOut.myPower = aJson[ "power" ].get< float >();
        }
        if ( aJson.contains( "hbaoDirections" ) ) {
            aOut.myHbaoDirections = aJson[ "hbaoDirections" ].get< uint32_t >();
        }
        if ( aJson.contains( "hbaoSteps" ) ) {
            aOut.myHbaoSteps = aJson[ "hbaoSteps" ].get< uint32_t >();
        }
        if ( aJson.contains( "gtaoSlices" ) ) {
            aOut.myGtaoSlices = aJson[ "gtaoSlices" ].get< uint32_t >();
        }
        if ( aJson.contains( "gtaoStepsPerSlice" ) ) {
            aOut.myGtaoStepsPerSlice = aJson[ "gtaoStepsPerSlice" ].get< uint32_t >();
        }
        if ( aJson.contains( "gtaoFalloff" ) ) {
            aOut.myGtaoFalloff = aJson[ "gtaoFalloff" ].get< float >();
        }
        if ( aJson.contains( "upsampleDepthSigma" ) ) {
            aOut.myUpsampleDepthSigma = aJson[ "upsampleDepthSigma" ].get< float >();
        }
        if ( aJson.contains( "normalAwareRadius" ) ) {
            aOut.myNormalAwareRadius = aJson[ "normalAwareRadius" ].get< float >();
        }
        if ( aJson.contains( "temporalEnabled" ) ) {
            aOut.myTemporalEnabled = aJson[ "temporalEnabled" ].get< bool >();
        }
        if ( aJson.contains( "temporalBlend" ) ) {
            aOut.myTemporalBlend = aJson[ "temporalBlend" ].get< float >();
        }
        if ( aJson.contains( "contactSoftEnabled" ) ) {
            aOut.myContactSoftEnabled = aJson[ "contactSoftEnabled" ].get< bool >();
        }
        if ( aJson.contains( "contactSoftBlurRadius" ) ) {
            aOut.myContactSoftBlurRadius = aJson[ "contactSoftBlurRadius" ].get< float >();
        }
        if ( aJson.contains( "contactSoftDepthSigma" ) ) {
            aOut.myContactSoftDepthSigma = aJson[ "contactSoftDepthSigma" ].get< float >();
        }
    }

    nlohmann::json PostToJson( const Gfx_PostSettings& aPost ) {
        return nlohmann::json{ { "exposure", aPost.myExposure },
                               { "tonemapEnabled", aPost.myTonemapEnabled },
                               { "bloomEnabled", aPost.myBloomEnabled },
                               { "bloomThreshold", aPost.myBloomThreshold },
                               { "bloomIntensity", aPost.myBloomIntensity },
                               { "tonemapMode", aPost.myTonemapMode } };
    }

    void PostFromJson( const nlohmann::json& aJson, Gfx_PostSettings& aOut ) {
        if ( aJson.contains( "exposure" ) ) {
            aOut.myExposure = aJson[ "exposure" ].get< float >();
        }
        if ( aJson.contains( "tonemapEnabled" ) ) {
            aOut.myTonemapEnabled = aJson[ "tonemapEnabled" ].get< bool >();
        }
        if ( aJson.contains( "bloomEnabled" ) ) {
            aOut.myBloomEnabled = aJson[ "bloomEnabled" ].get< bool >();
        }
        if ( aJson.contains( "bloomThreshold" ) ) {
            aOut.myBloomThreshold = aJson[ "bloomThreshold" ].get< float >();
        }
        if ( aJson.contains( "bloomIntensity" ) ) {
            aOut.myBloomIntensity = aJson[ "bloomIntensity" ].get< float >();
        }
        if ( aJson.contains( "tonemapMode" ) ) {
            aOut.myTonemapMode = aJson[ "tonemapMode" ].get< int >();
        }
    }

}  // namespace

std::filesystem::path DefaultPath( const std::filesystem::path& aAssetRoot ) {
    return aAssetRoot / "Config" / "user-tuning.json";
}

Snapshot Capture( Vk_Renderer& aRenderer, const Util_CameraSettings& aCamera, const ViewportToggles& aViewport ) {
    const GpuEnvironmentData& env = aRenderer.GetEnvironmentData();
    Snapshot                  snap{};
    snap.myAmbientColor      = env.myAmbientColor;
    snap.mySunlightColor     = env.mySunlightColor;
    snap.mySunlightDirection = env.mySunlightDirection;
    snap.myLighting          = aRenderer.GetLightingSettings();
    snap.myAo                = aRenderer.GetAoSettings();
    snap.myPost              = aRenderer.GetPostSettings();
    snap.myCamera            = aCamera;
    snap.myViewport          = aViewport;
    return snap;
}

void Apply( const Snapshot& aSnapshot, Vk_Renderer& aRenderer, Util_CameraSettings& aCamera, ViewportToggles& aViewport ) {
    GpuEnvironmentData& env             = aRenderer.GetEnvironmentData();
    const float         debugViewPacked = env.myFogDistance.w;  // session-only; owned by Render debug tab
    env.myAmbientColor                  = aSnapshot.myAmbientColor;
    env.mySunlightColor                 = aSnapshot.mySunlightColor;
    env.mySunlightDirection             = aSnapshot.mySunlightDirection;
    env.myFogDistance.w                 = debugViewPacked;

    aRenderer.GetLightingSettings() = aSnapshot.myLighting;
    aRenderer.GetAoSettings()       = aSnapshot.myAo;
    aRenderer.GetPostSettings()     = aSnapshot.myPost;
    aCamera                         = aSnapshot.myCamera;
    aViewport                       = aSnapshot.myViewport;
}

bool LoadFromFile( const std::filesystem::path& aPath, Snapshot& aOut ) {
    std::ifstream file( aPath );
    if ( !file.is_open() ) {
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    }
    catch ( const nlohmann::json::exception& e ) {
        throw std::runtime_error( std::string( "Invalid tuning JSON: " ) + aPath.string() + " — " + e.what() );
    }

    if ( !root.contains( "version" ) || root[ "version" ].get< int >() != kTuningSchemaVersion ) {
        throw std::runtime_error( "Unsupported tuning schema in: " + aPath.string() );
    }

    if ( root.contains( "environment" ) && root[ "environment" ].is_object() ) {
        const auto& environment = root[ "environment" ];
        if ( environment.contains( "ambientColor" ) ) {
            aOut.myAmbientColor = JsonToVec4( environment[ "ambientColor" ] );
        }
        if ( environment.contains( "sunlightColor" ) ) {
            aOut.mySunlightColor = JsonToVec4( environment[ "sunlightColor" ] );
        }
        if ( environment.contains( "sunlightDirection" ) ) {
            aOut.mySunlightDirection = JsonToVec4( environment[ "sunlightDirection" ] );
        }
    }

    if ( root.contains( "lighting" ) ) {
        LightingFromJson( root[ "lighting" ], aOut.myLighting );
    }
    if ( root.contains( "ao" ) ) {
        AoFromJson( root[ "ao" ], aOut.myAo );
    }
    if ( root.contains( "post" ) ) {
        PostFromJson( root[ "post" ], aOut.myPost );
    }
    if ( root.contains( "camera" ) && root[ "camera" ].is_object() ) {
        const auto& camera = root[ "camera" ];
        if ( camera.contains( "moveSpeed" ) ) {
            aOut.myCamera.myMoveSpeed = camera[ "moveSpeed" ].get< float >();
        }
        if ( camera.contains( "mouseSensitivity" ) ) {
            aOut.myCamera.myMouseSensitivity = camera[ "mouseSensitivity" ].get< float >();
        }
    }
    if ( root.contains( "viewport" ) && root[ "viewport" ].is_object() ) {
        const auto& viewport = root[ "viewport" ];
        if ( viewport.contains( "sunGizmo" ) ) {
            aOut.myViewport.mySunGizmo = viewport[ "sunGizmo" ].get< bool >();
        }
        if ( viewport.contains( "ddgiVolumeBounds" ) ) {
            aOut.myViewport.myDdgiVolumeBounds = viewport[ "ddgiVolumeBounds" ].get< bool >();
        }
    }

    return true;
}

void SaveToFile( const std::filesystem::path& aPath, const Snapshot& aSnapshot ) {
    const nlohmann::json root = { { "version", kTuningSchemaVersion },
                                  { "environment",
                                    { { "ambientColor", Vec4ToJson( aSnapshot.myAmbientColor ) },
                                      { "sunlightColor", Vec4ToJson( aSnapshot.mySunlightColor ) },
                                      { "sunlightDirection", Vec4ToJson( aSnapshot.mySunlightDirection ) } } },
                                  { "lighting", LightingToJson( aSnapshot.myLighting ) },
                                  { "ao", AoToJson( aSnapshot.myAo ) },
                                  { "post", PostToJson( aSnapshot.myPost ) },
                                  { "camera", { { "moveSpeed", aSnapshot.myCamera.myMoveSpeed }, { "mouseSensitivity", aSnapshot.myCamera.myMouseSensitivity } } },
                                  { "viewport", { { "sunGizmo", aSnapshot.myViewport.mySunGizmo }, { "ddgiVolumeBounds", aSnapshot.myViewport.myDdgiVolumeBounds } } } };

    std::error_code ec;
    std::filesystem::create_directories( aPath.parent_path(), ec );

    std::ofstream file( aPath );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Could not write tuning file: " + aPath.string() );
    }
    file << root.dump( 2 ) << '\n';
}

void ResetRendererTuning( const Util_EngineConfig& aConfig, Vk_Renderer& aRenderer, Util_CameraSettings& aCamera, ViewportToggles& aViewport ) {
    const std::filesystem::path path = DefaultPath( aConfig.GetAssetRoot() );
    std::error_code             ec;
    std::filesystem::remove( path, ec );

    aRenderer.GetLightingSettings() = aConfig.GetLightingSettings();
    aRenderer.GetAoSettings()       = Gfx_AoSettings{};
    aRenderer.GetPostSettings()     = Gfx_PostSettings{};
    aCamera                         = Util_CameraSettings{};
    aViewport                       = ViewportToggles{};

    UtilLogger::Info( "CONFIG", "Reset renderer tuning (user-tuning.json removed if present)." );
}

}  // namespace Util_TuningPrefs
