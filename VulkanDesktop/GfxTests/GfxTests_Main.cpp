// GfxTests: CPU-only regression tests for Gfx draw-stream (no Vulkan / GLFW).

#include "../Gfx/Gfx_ClusterLighting.h"
#include "../Gfx/Gfx_DrawBatch.h"
#include "../Gfx/Gfx_DrawCullSort.h"
#include "../Gfx/Gfx_DrawExtract.h"
#include "../Gfx/Gfx_EntityGpuRecord.h"
#include "../Gfx/Gfx_FrameDrawStream.h"
#include "../Gfx/Gfx_GpuCull.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_Lod.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderPipeline.h"
#include "../Gfx/Gfx_RenderPreset.h"
#include "../Gfx/Gfx_SceneSoA.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../Gfx/Gfx_TemporalJitter.h"
#include "../RenderCore/Vk_DescriptorPolicy.h"
#include "../RenderCore/Vk_RhiBackend.h"
#include "../RenderCore/Vk_RhiDevice.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_ResolvePath.h"

#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#if defined( _WIN32 )
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {

int gFailures = 0;

void Expect( bool aCondition, const char* aMessage ) {
    if ( !aCondition ) {
        std::cerr << "[FAIL] " << aMessage << '\n';
        std::cerr.flush();
        ++gFailures;
    }
}

std::filesystem::path FindRepoRootFromExe() {
    std::filesystem::path dir = std::filesystem::absolute( std::filesystem::path( "." ) );
#if defined( _WIN32 )
    wchar_t modulePath[ MAX_PATH ]{};
    if ( GetModuleFileNameW( nullptr, modulePath, MAX_PATH ) != 0 ) {
        dir = std::filesystem::path( modulePath ).parent_path();
    }
#endif
    for ( int i = 0; i < 12; ++i ) {
        if ( std::filesystem::exists( dir / "VulkanDesktop.sln" ) ) {
            return dir;
        }
        if ( !dir.has_parent_path() || dir == dir.parent_path() ) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
}

Util_EngineConfig LoadConfigFromArgv( int aArgc, char** aArgv ) {
    Util_EngineConfig config;
    config.LoadFromArgv( aArgc, aArgv );
    return config;
}

void InitTestConfig( const std::filesystem::path& aRepoRoot, Util_EngineConfig& aOutConfig ) {
    static std::vector< char > rootArgStorage;
    const std::string          rootStr = aRepoRoot.string();
    rootArgStorage.assign( rootStr.begin(), rootStr.end() );
    rootArgStorage.push_back( '\0' );

    static char arg0[] = "GfxTests";
    static char arg1[] = "--asset-root";
    char*       arg2   = rootArgStorage.data();

    char* argv[] = { arg0, arg1, arg2 };
    aOutConfig.LoadFromArgv( 3, argv );
    Gfx_ShaderPermutation::Initialize( UtilResolvePath::Resolve( aOutConfig, Gfx_ShaderPermutation::kRegistryLogicalPath ) );
    Gfx_ShaderPermutation::SetActiveByName( aOutConfig.GetShaderPermutationName() );
}

void TestConfigPrecedence( const std::filesystem::path& aRepoRoot ) {
    const std::filesystem::path jsonPath = aRepoRoot / "Config" / "engine.gfxtests_precedence.json";
    {
        std::ofstream out( jsonPath );
        if ( !out.is_open() ) {
            Expect( false, "config precedence: write temp json" );
            return;
        }
        out << R"({ "vsync": true, "assetRoot": "" })";
    }

    static std::vector< char > configPathStorage;
    const std::string          configStr = jsonPath.string();
    configPathStorage.assign( configStr.begin(), configStr.end() );
    configPathStorage.push_back( '\0' );

    static std::vector< char > rootArgStorage;
    const std::string          rootStr = aRepoRoot.string();
    rootArgStorage.assign( rootStr.begin(), rootStr.end() );
    rootArgStorage.push_back( '\0' );

    static char arg0[]          = "GfxTests";
    static char argConfigFlag[] = "--config";
    static char argNoVsync[]    = "--no-vsync";
    static char argAssetFlag[]  = "--asset-root";

    {
        char*             argv[]   = { arg0, argConfigFlag, configPathStorage.data() };
        Util_EngineConfig fromJson = LoadConfigFromArgv( 3, argv );
        Expect( fromJson.GetVsync(), "config precedence: json vsync true" );
    }

    {
        char*             argv[]   = { arg0, argConfigFlag, configPathStorage.data(), argNoVsync };
        Util_EngineConfig cliVsync = LoadConfigFromArgv( 4, argv );
        Expect( !cliVsync.GetVsync(), "config precedence: CLI --no-vsync wins" );
    }

    {
        char*             argAsset = rootArgStorage.data();
        char*             argv[]   = { arg0, argConfigFlag, configPathStorage.data(), argAssetFlag, argAsset };
        Util_EngineConfig cliRoot  = LoadConfigFromArgv( 5, argv );
        Expect( cliRoot.GetAssetRoot() == std::filesystem::weakly_canonical( aRepoRoot ), "config precedence: CLI --asset-root wins" );
    }

    std::error_code ec;
    std::filesystem::remove( jsonPath, ec );
}

glm::mat4 Mat4FromColumnMajor( const float ( &aValues )[ 16 ] ) {
    return glm::make_mat4( aValues );
}

void PopulateDemoSceneSoA( Gfx_SceneSoA& aScene ) {
    aScene.Clear();

    struct EntitySpec {
        uint32_t        myLogicalMesh;
        uint32_t        myMaterial;
        Gfx_RenderFlags myFlags;
        float           myTransform[ 16 ];
    };

    // Material ids follow demo.json materials[] order: viking=0, monkey=1, transparent=2, rock=3, grass=4, metal=5, wood=6.
    const EntitySpec entities[] = {
        { 0, 0, Gfx_RenderOpaque, { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -4, 0, 0, 1 } },
        { 1, 1, Gfx_RenderOpaque, { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 0, 0, 1 } },
        { 1, 2, Gfx_RenderTransparent, { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1.5f, 1 } },
        { 2, 4, Gfx_RenderOpaque, { 4, 0, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, -6, 0, -1, 1 } },
        { 2, 4, Gfx_RenderOpaque, { 4, 0, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 0, 0, -16, 1 } },
        { 3, 3, Gfx_RenderOpaque, { 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, 0, 0, -6.5f, 1 } },
        { 4, 6, Gfx_RenderOpaque, { 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, -3, 0, -4, 1 } },
        { 5, 5, Gfx_RenderOpaque, { 2.5f, 0, 0, 0, 0, 2.5f, 0, 0, 0, 0, 2.5f, 0, 3, 0, -4, 1 } },
        { 6, 4, Gfx_RenderOpaque, { 2.5f, 0, 0, 0, 0, 2.5f, 0, 0, 0, 0, 2.5f, 0, 6.5f, 0, -3, 1 } },
    };

    for ( const EntitySpec& spec : entities ) {
        aScene.AllocEntity( spec.myLogicalMesh, spec.myMaterial, Mat4FromColumnMajor( spec.myTransform ), 0xFFFFFFFFu, spec.myFlags );
    }
}

Gfx_CullViewParams MakePerspectiveView( const glm::vec3& aEye, const glm::vec3& aCenter, float aFovDegrees, float aAspect ) {
    const glm::vec3 up{ 0.0f, 0.0f, 1.0f };

    Gfx_CullViewParams view{};
    view.myView = glm::lookAt( aEye, aCenter, up );
    view.myProj = glm::perspective( glm::radians( aFovDegrees ), aAspect, 0.1f, 200.0f );
    view.myProj[ 1 ][ 1 ] *= -1.0f;
    return view;
}

Gfx_CullViewParams BuildDemoOverviewView() {
    return MakePerspectiveView( glm::vec3( 10.0f, 8.0f, 10.0f ), glm::vec3( 0.0f, 0.0f, -4.0f ), 50.0f, 1600.0f / 1200.0f );
}

// Tight FOV on viking room — distant demo entities (e.g. slot 4 @ z=-16) should fail frustum cull.
Gfx_CullViewParams BuildDemoCloseVikingView() {
    return MakePerspectiveView( glm::vec3( -3.5f, 0.5f, 1.0f ), glm::vec3( -4.0f, 0.0f, 0.0f ), 35.0f, 1600.0f / 1200.0f );
}

void TestSoAGeneration() {
    Gfx_SceneSoA scene;
    const auto   id1 = scene.AllocEntity( 0, 0, glm::mat4( 1.0f ) );
    Expect( scene.IsAlive( id1 ), "SoA: id1 alive" );

    Expect( scene.FreeEntity( id1 ), "SoA: free id1" );
    Expect( !scene.IsAlive( id1 ), "SoA: stale id1 not alive" );

    const auto id2 = scene.AllocEntity( 1, 1, glm::mat4( 1.0f ) );
    Expect( scene.IsAlive( id2 ), "SoA: id2 alive after slot reuse" );
    Expect( id2.myIndex == id1.myIndex, "SoA: reused slot index" );
    Expect( id2.myGeneration != id1.myGeneration, "SoA: generation bumped on reuse" );
}

void TestTransparentSortStableUnderRotation() {
    Gfx_SceneSoA scene;

    const glm::mat4 nearTransform = glm::translate( glm::mat4( 1.0f ), glm::vec3( 0.0f, 0.0f, 3.0f ) );
    const glm::mat4 farTransform  = glm::translate( glm::mat4( 1.0f ), glm::vec3( 0.0f, 0.0f, 8.0f ) );
    scene.AllocEntity( 0, 0, nearTransform, 0xFFFFFFFFu, Gfx_RenderTransparent );
    scene.AllocEntity( 0, 0, farTransform, 0xFFFFFFFFu, Gfx_RenderTransparent );

    Gfx_Bounds offsetLocalBounds{};
    offsetLocalBounds.myMin = glm::vec3( 0.0f );
    offsetLocalBounds.myMax = glm::vec3( 1.0f );
    scene.SetLocalBoundsForSlot( 0, offsetLocalBounds );
    scene.SetLocalBoundsForSlot( 1, offsetLocalBounds );

    const glm::mat4 view = glm::lookAt( glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

    auto sortTransparentDraws = [ & ]( const glm::mat4& aNearTransform, const glm::mat4& aFarTransform ) {
        scene.SetWorldTransform( 0, aNearTransform );
        scene.SetWorldTransform( 1, aFarTransform );

        Gfx_FrameExtract   extract{};
        Gfx_CullViewParams viewParams{};
        viewParams.myView          = view;
        viewParams.myProj          = glm::mat4( 1.0f );
        viewParams.myViewLayerMask = 0xFFFFFFFFu;
        Gfx_ExtractDrawInstances( scene, viewParams, extract );
        Gfx_SortTransparentDrawInstances( extract.myTransparent, scene, view );
        return extract.myTransparent.myDrawInstances;
    };

    const auto referenceOrder = sortTransparentDraws( nearTransform, farTransform );
    Expect( referenceOrder.size() == 2, "transparent sort: expected two draws" );
    Expect( referenceOrder.front().myEntityIndex == 1, "transparent sort: farther entity first (back-to-front)" );

    const glm::mat4 rotatedNear  = nearTransform * glm::rotate( glm::mat4( 1.0f ), glm::radians( 90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );
    const auto      rotatedOrder = sortTransparentDraws( rotatedNear, farTransform );
    Expect( rotatedOrder.size() == 2, "transparent sort: rotated case count" );
    Expect( rotatedOrder[ 0 ].myEntityIndex == referenceOrder[ 0 ].myEntityIndex && rotatedOrder[ 1 ].myEntityIndex == referenceOrder[ 1 ].myEntityIndex,
            "transparent sort stable when entity rotates around pivot (bounds-center eye Z)" );
}

void TestDirectionalLightViewOrientation() {
    const glm::vec3 sunDir = glm::normalize( glm::vec3( 0.3f, 0.2f, 0.9f ) );
    const glm::vec3 focus( 5.0f, -2.0f, 1.0f );

    const glm::mat4 lightView = Gfx_LightingMath::Gfx_ComputeDirectionalLightView( sunDir, focus, 64.0f );

    // Scene-side point (opposite sun) must lie in front of the light camera so depth is captured along incoming light.
    const glm::vec3 sceneSide      = focus - sunDir * 80.0f;
    const float     sceneSideViewZ = ( lightView * glm::vec4( sceneSide, 1.0f ) ).z;
    Expect( sceneSideViewZ < 0.0f, "scene-side point should be in front of directional light view" );

    // A point beyond the virtual light eye (sun side) should be behind the camera.
    const glm::vec3 beyondEye      = focus + sunDir * 80.0f;
    const float     beyondEyeViewZ = ( lightView * glm::vec4( beyondEye, 1.0f ) ).z;
    Expect( beyondEyeViewZ > 0.0f, "point beyond light eye should be behind directional light view" );
}

void TestVulkanViewportClipZContract() {
    // Vulkan viewport: z_fb = z_ndc * (maxDepth - minDepth) + minDepth. Default range [0,1] ⇒ identity.
    Expect( Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 0.0f ) == 0.0f, "viewport [0,1]: clip 0 -> fb 0" );
    Expect( Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 0.5f ) == 0.5f, "viewport [0,1]: clip 0.5 -> fb 0.5" );
    Expect( Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 1.0f ) == 1.0f, "viewport [0,1]: clip 1 -> fb 1" );
    Expect( Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 0.25f, 0.0f, 1.0f ) == 0.25f, "explicit [0,1] range" );

    // Anti-regression: OpenGL-style Z remap must NOT be used for Vulkan ZO clip Z.
    constexpr float kOpenGlRemapMid = 0.5f * 0.5f + 0.5f;  // 0.75
    Expect( Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 0.5f ) != kOpenGlRemapMid, "must not apply OpenGL clipZ*0.5+0.5" );
    Expect( Gfx_LightingMath::Gfx_MapClipZToShadowCompareDepth( 0.5f ) == Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 0.5f ),
            "shadow compare depth matches framebuffer depth helper" );

    // Non-default viewport range still uses the linear Vulkan formula (not OpenGL remap).
    Expect( Gfx_LightingMath::Gfx_ClipZToFramebufferDepth( 0.5f, 0.0f, 2.0f ) == 1.0f, "custom depth range mid" );
}

void TestDirectionalShadowSetupForSmallScene() {
    const glm::vec3 defaultSun = Gfx_LightingMath::Gfx_DefaultSunDirectionTowardLight();
    Expect( defaultSun.z > 0.5f, "default sun should be mostly overhead (+Z) for Z-up scenes" );

    Gfx_Bounds scene{};
    scene.myMin = glm::vec3( -0.08f, -0.12f, 0.0f );
    scene.myMax = glm::vec3( 0.08f, 0.12f, 0.16f );

    constexpr uint32_t                                 shadowMapSize = 2048u;
    const Gfx_LightingMath::Gfx_DirectionalShadowSetup setup         = Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowSetup( defaultSun, scene, shadowMapSize );

    Expect( setup.myLightSpaceDepthRange < 2.0f, "Sponza-scale shadow ortho depth range should stay scene-sized" );

    const float bias = Gfx_LightingMath::Gfx_ComputeShadowDepthBiasConstant( setup.myLightSpaceDepthRange );
    Expect( bias == -1.4f, "directional shadow depth bias should use Khronos fixed constant" );

    const Gfx_LightingMath::Gfx_DirectionalShadowSetup setupAgain = Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowSetup( defaultSun, scene, shadowMapSize );
    Expect( setup.myLightViewProj == setupAgain.myLightViewProj, "scene-only shadow matrix must be stable across recomputation" );

    // Reverse-Z ZO: receiver near the light-facing scene side -> clipZ ~1; far side along -sun -> clipZ closer to 0.
    const glm::vec3 focusCenter = Gfx_BoundsCenter( scene );
    const glm::vec3 nearLit     = focusCenter + defaultSun * 0.02f;
    const glm::vec3 farLit      = focusCenter - defaultSun * 0.12f;
    const auto      clipZOf     = [ & ]( const glm::vec3& p ) {
        const glm::vec4 clip = setup.myLightViewProj * glm::vec4( p, 1.0f );
        return clip.z / clip.w;
    };
    const float nearClipZ = clipZOf( nearLit );
    const float farClipZ  = clipZOf( farLit );
    Expect( nearClipZ > 0.5f && nearClipZ <= 1.0f + 1e-3f, "reverse-Z ZO: sun-side point maps toward clipZ 1" );
    Expect( farClipZ >= -1e-3f && farClipZ < nearClipZ, "reverse-Z ZO: opposite-sun point has smaller clipZ than sun-side" );
    Expect( Gfx_LightingMath::Gfx_MapClipZToShadowCompareDepth( nearClipZ ) == nearClipZ, "compare depth identity for ZO clip Z" );
}

void TestSunElevationShadowGate() {
    const glm::vec3 overheadSun = Gfx_LightingMath::Gfx_DefaultSunDirectionTowardLight();
    const glm::vec3 belowSun    = glm::normalize( glm::vec3( 0.0f, 0.0f, -1.0f ) );

    Expect( Gfx_LightingMath::Gfx_IsSunElevationValidForShadows( overheadSun ), "overhead sun valid for shadow compare" );
    Expect( !Gfx_LightingMath::Gfx_IsSunElevationValidForShadows( belowSun ), "below-horizon sun rejected for shadow compare" );
    Expect( Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( true, overheadSun ), "shadows enabled + overhead sun compares" );
    Expect( !Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( true, belowSun ), "shadows enabled + below sun skips compare" );
    Expect( !Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( false, overheadSun ), "shadows disabled skips compare" );

    Gfx_LightingSettings settings{};
    settings.myShadowsEnabled         = true;
    settings.myIblEnabled             = true;
    const Gpu_LightingGlobals globals = Gfx_BuildLightingGlobals( settings, glm::mat4( 1.0f ), 0.0f, belowSun, 2048u );
    Expect( globals.myShadowParams.z == 0.0f, "below-horizon sun clears shadow compare flag" );
    Expect( globals.myShadowParams.w > 0.0f, "shadow PCF texel stride uploaded" );
}

void TestEntityGpuRecordSync() {
    Gfx_SceneSoA scene;

    const glm::mat4 worldTransform = glm::translate( glm::mat4( 1.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) );
    const auto      id             = scene.AllocEntity( 2, 3, worldTransform );

    Gfx_Bounds localBounds{};
    localBounds.myMin = glm::vec3( -1.0f );
    localBounds.myMax = glm::vec3( 1.0f );
    scene.SetLocalBoundsForSlot( id.myIndex, localBounds );

    Gpu_EntityRecord record{};
    Gfx_FillEntityGpuRecord( record, scene, id.myIndex, 99 );

    Expect( record.myLayerMask != 0, "entity record: active slot has layer mask" );
    Expect( record.myLogicalMeshId == 2, "entity record: logical mesh id" );
    Expect( record.myMaterialId == 3, "entity record: material id" );
    Expect( record.myIndirect.indexCount == 99, "entity record: index count" );
    Expect( record.myBoundsMin.x == 0.0f && record.myBoundsMax.x == 2.0f, "entity record: world AABB synced from SoA" );

    scene.FreeEntity( id );
    Gfx_FillEntityGpuRecord( record, scene, id.myIndex, 99 );
    Expect( record.myLayerMask == 0, "entity record: freed slot cleared" );
}

void TestEntityIndirectSlot() {
    const uint32_t viewBase = 128;
    const uint32_t entity   = 5;
    Expect( Gfx_ComputeEntityIndirectSlot( viewBase, entity ) == viewBase + entity, "entity indirect slot matches outputBaseSlot + entity" );
    Expect( Gfx_ComputeEntityIndirectSlot( 0, 0 ) == 0, "entity indirect slot zero base" );
}

void TestCpuGpuCullParity( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, const char* aCaseName ) {
    if ( Gfx_AreCpuGpuCullVisibleSlotsEqual( aScene, aView ) ) {
        return;
    }

    std::vector< uint32_t > cpuSlots;
    std::vector< uint32_t > gpuSlots;
    Gfx_CollectCpuCullVisibleSlots( aScene, aView, cpuSlots );
    Gfx_CollectGpuCullVisibleSlots( aScene, aView, gpuSlots );
    std::cerr << "[FAIL] cpu/gpu cull parity (" << aCaseName << "): cpu=" << cpuSlots.size() << " gpu=" << gpuSlots.size() << '\n';
    std::cerr.flush();
    ++gFailures;
}

void TestCpuGpuCullParityDemoViews() {
    Gfx_SceneSoA scene;
    PopulateDemoSceneSoA( scene );

    TestCpuGpuCullParity( scene, BuildDemoOverviewView(), "cpu/gpu cull parity: demo overview (all visible)" );

    const Gfx_CullViewParams closeView = BuildDemoCloseVikingView();
    std::vector< uint32_t >  culledCpu;
    Gfx_CollectCpuCullVisibleSlots( scene, closeView, culledCpu );
    Expect( culledCpu.size() < scene.GetActiveCount(), "cpu/gpu cull parity: close view culls some entities" );
    TestCpuGpuCullParity( scene, closeView, "cpu/gpu cull parity: demo close viking view" );
}

void TestCpuGpuCullParityLayerMask() {
    Gfx_SceneSoA scene;
    scene.AllocEntity( 0, 0, glm::translate( glm::mat4( 1.0f ), glm::vec3( 0.0f, 0.0f, 2.0f ) ), 0x00000001u, Gfx_RenderOpaque );
    scene.AllocEntity( 0, 0, glm::translate( glm::mat4( 1.0f ), glm::vec3( 0.0f, 0.0f, 4.0f ) ), 0x00000002u, Gfx_RenderOpaque );

    Gfx_CullViewParams view = MakePerspectiveView( glm::vec3( 0.0f, -6.0f, 3.0f ), glm::vec3( 0.0f, 0.0f, 3.0f ), 60.0f, 1.0f );
    view.myViewLayerMask    = 0x00000002u;

    std::vector< uint32_t > visible;
    Gfx_CollectCpuCullVisibleSlots( scene, view, visible );
    Expect( visible.size() == 1 && visible.front() == 1u, "cpu/gpu cull parity: layer mask leaves one slot" );
    TestCpuGpuCullParity( scene, view, "cpu/gpu cull parity: layer mask filter" );
}

void TestLodGpuEntityRecordParity() {
    Gfx_SceneSoA scene;
    // Near entity: logical mesh 0 → LOD 0 (mesh 10). Far entity: logical mesh 0 → LOD 1 (mesh 20).
    scene.AllocEntity( 0, 0, glm::translate( glm::mat4( 1.0f ), glm::vec3( 0.0f, 0.0f, 2.0f ) ) );
    scene.AllocEntity( 0, 0, glm::translate( glm::mat4( 1.0f ), glm::vec3( 0.0f, 0.0f, 40.0f ) ) );

    Gfx_LodTable lodTable;
    Gfx_LodChain chain{};
    chain.myMeshIds            = { 10, 20 };
    chain.myDistanceThresholds = { 15.0f };
    lodTable.SetChain( 0, std::move( chain ) );

    const glm::vec3    eye{ 0.0f, -5.0f, 2.0f };
    Gfx_CullViewParams view = MakePerspectiveView( eye, glm::vec3( 0.0f, 0.0f, 2.0f ), 60.0f, 1.0f );

    Gfx_LodState     cpuLodState;
    Gfx_FrameExtract extract{};
    Gfx_ExtractDrawInstances( scene, view, extract );
    Gfx_ApplyLodToFrameExtract( scene, eye, lodTable, cpuLodState, extract );

    Expect( extract.myOpaque.myDrawInstances.size() == 2, "lod gpu parity: two extracted draws" );

    Gfx_EntityRecordLodParams entityLod{};
    entityLod.myLodEnabled = true;
    entityLod.myCameraEye  = eye;
    entityLod.myLodTable   = &lodTable;
    Gfx_LodState entityLodSnapshot;
    entityLod.myLodState = &entityLodSnapshot;

    Expect( Gfx_ResolveEntityRecordMeshId( scene, 0, entityLod ) == 10, "lod gpu parity: near slot selects LOD0 mesh" );
    Expect( Gfx_ResolveEntityRecordMeshId( scene, 1, entityLod ) == 20, "lod gpu parity: far slot selects LOD1 mesh" );

    for ( const Gfx_DrawInstance& draw : extract.myOpaque.myDrawInstances ) {
        const uint32_t resolvedMesh = Gfx_ResolveEntityRecordMeshId( scene, draw.myEntityIndex, entityLod );
        Expect( draw.myMeshId == resolvedMesh, "lod gpu parity: CPU draw mesh matches entity-record LOD resolve" );
    }
}

void TestGpuCullSkipsCpuFrustumCull() {
    Gfx_SceneSoA scene;
    PopulateDemoSceneSoA( scene );

    Gfx_LodTable              lodTable;
    Gfx_LodState              lodState;
    Gfx_FrameDrawStreamParams params{};
    params.myScene          = &scene;
    params.myView           = BuildDemoOverviewView();
    params.myCameraEye      = glm::vec3( 10.0f, 8.0f, 10.0f );
    params.myCameraView     = params.myView.myView;
    params.myLodTable       = &lodTable;
    params.myLodState       = &lodState;
    params.myGpuCullEnabled = true;

    Gfx_FrameDrawStreamOutput   out;
    Gfx_FrameDrawStreamLogState logs;
    Gfx_ResetFrameDrawStreamLogState( logs );
    Gfx_BuildFrameDrawStream( params, out, logs );

    Expect( out.myDrawCountBeforeCull == 9, "gpu cull mode: pre-cull draw count" );
    Expect( out.myExtract.myOpaque.myDrawInstances.size() + out.myExtract.myTransparent.myDrawInstances.size() == 9,
            "gpu cull mode: CPU frustum cull skipped (all extracted draws kept)" );
}

void TestDemoCullAndBatch() {
    Gfx_SceneSoA scene;
    PopulateDemoSceneSoA( scene );
    Expect( scene.GetActiveCount() == 9, "demo SoA entity count" );

    Gfx_LodTable              lodTable;
    Gfx_LodState              lodState;
    Gfx_FrameDrawStreamParams params{};
    params.myScene      = &scene;
    params.myView       = BuildDemoOverviewView();
    params.myCameraEye  = glm::vec3( 10.0f, 8.0f, 10.0f );
    params.myCameraView = params.myView.myView;
    params.myLodTable   = &lodTable;
    params.myLodState   = &lodState;

    Gfx_FrameDrawStreamOutput   out;
    Gfx_FrameDrawStreamLogState logs;
    Gfx_ResetFrameDrawStreamLogState( logs );
    Gfx_BuildFrameDrawStream( params, out, logs );

    const uint32_t visibleDraws = static_cast< uint32_t >( out.myExtract.myOpaque.myDrawInstances.size() + out.myExtract.myTransparent.myDrawInstances.size() );
    const uint32_t batchRuns    = static_cast< uint32_t >( out.myOpaqueBatchRuns.size() + out.myTransparentBatchRuns.size() );

    Expect( visibleDraws == 9, "demo visible draws after cull" );
    // Two tree entities share logical mesh + mat_grass → 7 opaque batch runs + 1 transparent (not 9 draw-sized runs).
    Expect( out.myOpaqueBatchRuns.size() == 7, "demo opaque batch runs" );
    Expect( out.myTransparentBatchRuns.size() == 1, "demo transparent batch run" );
    Expect( batchRuns == 8, "demo total batch runs (7+1)" );
    Expect( out.myExtract.myOpaque.myDrawInstances.size() == 8, "demo opaque draws" );
    Expect( out.myExtract.myTransparent.myDrawInstances.size() == 1, "demo transparent draws" );
}

void TestClusterIndexFromTile() {
    Expect( Gfx_ClusterLighting::ClusterIndexFromTile( 3, 5, 120, 68 ) == 3 + 5 * 120, "cluster index screen tile depth slice 0" );
    Expect( Gfx_ClusterLighting::ClusterIndexFromTile( 1, 2, 10, 8, 3 ) == 1 + 2 * 10 + 3 * 10 * 8, "cluster index with depth slice" );
}

void TestClusterGridCount() {
    const uint32_t tilesW = Gfx_ClusterLighting::TilesForExtent( 1920 );
    const uint32_t tilesH = Gfx_ClusterLighting::TilesForExtent( 1080 );
    Expect( tilesW == 120, "cluster tilesX for 1920px @16" );
    Expect( tilesH == 68, "cluster tilesY for 1080px @16" );
    Expect( Gfx_ClusterLighting::ClusterCount( 1920, 1080 ) == tilesW * tilesH * Gfx_ClusterLighting::kDepthSlices, "cluster count = tilesX * tilesY * depthSlices" );
    Expect( sizeof( Gpu_ClusterLightList ) == sizeof( uint32_t ) * ( 1 + Gfx_ClusterLighting::kMaxLightsPerCluster ), "Gpu_ClusterLightList std430 size" );
}

void TestSliceSceneAssets( const std::filesystem::path& aRepoRoot ) {
    const std::filesystem::path scenePath = aRepoRoot / "Data" / "Scenes" / "slice.json";
    std::ifstream               file( scenePath );
    Expect( file.good(), "slice.json readable" );
    if ( !file.good() ) {
        return;
    }

    nlohmann::json root = nlohmann::json::parse( file );
    Expect( root.value( "name", "" ) == "slice", "slice scene name" );
    Expect( root.contains( "objective" ) && root[ "objective" ].value( "type", "" ) == "reach", "slice reach objective" );
    Expect( root[ "entities" ].is_array() && root[ "entities" ].size() == 7, "slice entity count" );

    std::unordered_set< std::string > paths;
    auto                              addPath = [ & ]( const std::string& aPath ) {
        if ( !aPath.empty() ) {
            paths.insert( aPath );
        }
    };
    for ( const auto& mesh : root[ "meshes" ] ) {
        addPath( mesh.value( "path", "" ) );
    }
    for ( const auto& texture : root[ "textures" ] ) {
        addPath( texture.value( "path", "" ) );
    }
    const auto& lit = root[ "shaders" ][ "lit" ];
    addPath( lit.value( "vert", "" ) );
    addPath( lit.value( "frag", "" ) );

    for ( const std::string& path : paths ) {
        const std::filesystem::path full = aRepoRoot / std::filesystem::path( path );
        Expect( std::filesystem::exists( full ), ( "slice asset exists: " + path ).c_str() );
    }
}

void TestRenderPresetHybridDeferred() {
    Expect( Gfx_RenderPreset::IsHybridDeferred( "HybridDeferred" ), "HybridDeferred preset recognized" );
    Expect( !Gfx_RenderPreset::IsHybridDeferred( "ForwardLit" ), "ForwardLit is not hybrid deferred" );
    Expect( Gfx_RenderPreset::ToShaderPermutationName( "HybridDeferred" ) == "lit", "HybridDeferred maps to lit permutation for scene materials" );
    bool threw = false;
    try {
        ( void )Gfx_RenderPreset::ToShaderPermutationName( "NotAPreset" );
    }
    catch ( const std::exception& ) {
        threw = true;
    }
    Expect( threw, "unknown render preset throws" );
}

void TestHybridDeferredFramePlan() {
    Gfx_PipelineEnableFlags flags{};
    flags.myShadow              = true;
    flags.myGBuffer             = true;
    flags.myClusterBuild        = true;
    flags.myDepthPyramid        = true;
    flags.mySsr                 = true;
    flags.myAo                  = true;
    flags.myDdgiProbeUpdate     = false;
    flags.myShadowAoSoft        = false;
    flags.myDeferredTransparent = true;
    flags.myPost                = true;

    const Gfx_FramePlan plan = Gfx_RenderPipeline::BuildHybridDeferred( flags );
    Expect( plan.myNodes.size() == static_cast< size_t >( Gfx_PassId::Count ), "hybrid plan has all pass nodes" );
    Expect( plan.myOrdered.size() == plan.myNodes.size(), "ordered size matches nodes" );

    auto indexOf = [ & ]( Gfx_PassId aId ) -> size_t {
        for ( size_t i = 0; i < plan.myOrdered.size(); ++i ) {
            if ( plan.myOrdered[ i ] == aId ) {
                return i;
            }
        }
        return plan.myOrdered.size();
    };
    Expect( indexOf( Gfx_PassId::Shadow ) < indexOf( Gfx_PassId::GBuffer ), "Shadow before GBuffer" );
    Expect( indexOf( Gfx_PassId::GBuffer ) < indexOf( Gfx_PassId::ClusterBuild ), "GBuffer before ClusterBuild" );
    Expect( indexOf( Gfx_PassId::DepthPyramid ) < indexOf( Gfx_PassId::SSAO ), "DepthPyramid before AO" );
    Expect( indexOf( Gfx_PassId::DeferredTransparent ) < indexOf( Gfx_PassId::Post ), "Deferred before Post" );

    bool aoEnabled   = false;
    bool ddgiEnabled = false;
    for ( const Gfx_FramePlanNode& node : plan.myNodes ) {
        if ( node.myId == Gfx_PassId::SSAO ) {
            aoEnabled = node.myEnabled;
        }
        if ( node.myId == Gfx_PassId::DdgiProbeUpdate ) {
            ddgiEnabled = node.myEnabled;
        }
    }
    Expect( aoEnabled, "AO enable flag propagated" );
    Expect( !ddgiEnabled, "DDGI disabled flag propagated" );
}

void TestResolveEnableFlagsFromBuildInput() {
    Gfx_PipelineBuildInput input{};
    input.myLighting.myShadowsEnabled = true;
    input.myLighting.myDdgiEnabled    = true;
    input.myAo.myEnabled              = true;
    input.myAo.myContactSoftEnabled   = true;
    input.mySunDirectionTowardLight   = Gfx_LightingMath::Gfx_DefaultSunDirectionTowardLight();
    input.myReady.myShadowMap         = true;
    input.myReady.myDepthPyramid      = true;
    input.myReady.mySsr               = true;
    input.myReady.myAo                = true;
    input.myReady.myShadowAoSoft      = true;
    input.myReady.myDeferredLighting  = true;
    input.myReady.myPostHybridResolve = true;

    Gfx_PipelineEnableFlags flags = Gfx_RenderPipeline::ResolveEnableFlags( input );
    Expect( flags.myShadow, "shadow enabled when ready + sun valid" );
    Expect( flags.myAo, "AO enabled when settings+ready+HiZ" );
    Expect( flags.mySsr, "SSR enabled when ready+HiZ" );
    Expect( flags.myDdgiProbeUpdate, "DDGI when ready+settings" );
    Expect( flags.myShadowAoSoft, "soft when ready+contactSoft" );
    Expect( flags.myPost, "post when hybrid resolve ready" );

    input.myAo.myEnabled = false;
    flags                = Gfx_RenderPipeline::ResolveEnableFlags( input );
    Expect( !flags.myAo, "AO off when settings disabled" );

    input.myAo.myEnabled         = true;
    input.myReady.myDepthPyramid = false;
    flags                        = Gfx_RenderPipeline::ResolveEnableFlags( input );
    Expect( !flags.myAo, "AO off without HiZ readiness" );
    Expect( !flags.mySsr, "SSR off without HiZ readiness" );

    input.myReady.myDepthPyramid    = true;
    input.mySunDirectionTowardLight = glm::normalize( glm::vec3( 0.0f, 0.0f, -1.0f ) );
    flags                           = Gfx_RenderPipeline::ResolveEnableFlags( input );
    Expect( !flags.myShadow, "shadow off below-horizon sun" );
}

void TestTemporalHaltonJitter() {
    Expect( Gfx_TemporalJitter::Halton( 1u, 2u ) == 0.5f, "Halton(1,2) == 0.5" );
    Expect( std::abs( Gfx_TemporalJitter::Halton( 1u, 3u ) - ( 1.0f / 3.0f ) ) < 1e-6f, "Halton(1,3) == 1/3" );

    const glm::vec2 jitter0 = Gfx_TemporalJitter::SampleNdc( 0u, 1920u, 1080u );
    Expect( std::abs( jitter0.x ) < 1e-6f, "Halton index 0 x jitter centered at 0 NDC" );

    const glm::vec2 jitter1 = Gfx_TemporalJitter::SampleNdc( 1u, 1920u, 1080u );
    Expect( std::abs( jitter1.x ) > 1e-6f, "Halton index 1 x jitter non-zero" );

    glm::mat4       proj     = glm::perspective( glm::radians( 45.0f ), 16.0f / 9.0f, 0.1f, 100.0f );
    const glm::mat4 jittered = Gfx_TemporalJitter::ApplyToProjection( proj, jitter1 );
    Expect( jittered[ 2 ][ 0 ] != proj[ 2 ][ 0 ], "projection jitter modifies clip X" );
    Expect( jittered[ 2 ][ 1 ] != proj[ 2 ][ 1 ], "projection jitter modifies clip Y" );
}

void TestRhiDeviceHeadlessConstruct() {
    Vk_RhiDevice rhi;
    rhi.SetEnableValidationLayers( false, {} );
    Expect( rhi.CreateInstanceHeadless(), "RhiDevice headless VkInstance create" );
    Expect( rhi.IsInstanceCreated(), "RhiDevice instance handle non-null" );
    if ( rhi.IsInstanceCreated() ) {
        vkDestroyInstance( rhi.myDeviceCtx.myInstance, nullptr );
        rhi.myDeviceCtx.myInstance = VK_NULL_HANDLE;
        Expect( !rhi.IsInstanceCreated(), "RhiDevice instance destroyed" );
    }
}

void TestRhiOpaqueDeviceAndCommandList() {
    // Gfx-facing path: only Rhi/ headers (vulkan included elsewhere in this TU for legacy Vk_RhiDevice test).
    Rhi_Device device = Rhi::DeviceCreateHeadless( false );
    Expect( static_cast< bool >( device ), "Rhi DeviceCreateHeadless returns handle" );
    Expect( Rhi::DeviceIsValid( device ), "Rhi DeviceIsValid after headless create" );
    Expect( !Rhi::DeviceHasLogicalDevice( device ), "Headless Rhi device has no logical device" );

    Rhi_CommandList list = Rhi::DeviceCreateCommandList( device );
    Expect( static_cast< bool >( list ), "Rhi DeviceCreateCommandList returns handle" );
    Expect( Rhi::CommandListIsValid( list ), "Rhi CommandListIsValid" );
    Expect( !Rhi::CommandListIsRecordingReady( list ), "CommandList not recording-ready until backend bind" );

    // Destroy device while a command list still lives — shell stays until list is destroyed (refcount).
    Rhi::DeviceDestroy( device );
    Expect( !static_cast< bool >( device ), "DeviceDestroy clears handle" );
    Expect( Rhi::CommandListIsValid( list ), "CommandList remains valid after DeviceDestroy (refcount)" );

    Rhi::CommandListBeginDebugLabel( list, "GfxTests.Rhi" );
    Rhi::CommandListEndDebugLabel( list );

    Rhi::CommandListDestroy( list );
    Expect( !static_cast< bool >( list ), "CommandListDestroy clears handle" );

    // Resource create without logical device must fail closed.
    Rhi_Device      again = Rhi::DeviceCreateHeadless( false );
    Rhi::BufferDesc bufDesc{};
    bufDesc.mySizeBytes = 64;
    Rhi_Buffer buf      = Rhi::DeviceCreateBuffer( again, bufDesc );
    Expect( !static_cast< bool >( buf ), "CreateBuffer fails closed without logical device" );
    Rhi::DeviceDestroy( again );
}

void TestRhiGraphicsRecordingSurface() {
    // E4.6a: graphics recording APIs must compile and no-op safely without a bound Vk command buffer.
    Rhi_Device      device = Rhi::DeviceCreateHeadless( false );
    Rhi_CommandList list   = Rhi::DeviceCreateCommandList( device );

    const Rhi_RenderPass  rp = RhiVulkan::RenderPassAdopt( reinterpret_cast< VkRenderPass >( static_cast< uintptr_t >( 0x11 ) ) );
    const Rhi_Framebuffer fb = RhiVulkan::FramebufferAdopt( reinterpret_cast< VkFramebuffer >( static_cast< uintptr_t >( 0x22 ) ) );
    Expect( static_cast< bool >( rp ), "RenderPassAdopt yields handle" );
    Expect( static_cast< bool >( fb ), "FramebufferAdopt yields handle" );
    Expect( RhiVulkan::RenderPassGetVk( rp ) == reinterpret_cast< VkRenderPass >( static_cast< uintptr_t >( 0x11 ) ), "RenderPassGetVk round-trip" );
    Expect( RhiVulkan::FramebufferGetVk( fb ) == reinterpret_cast< VkFramebuffer >( static_cast< uintptr_t >( 0x22 ) ), "FramebufferGetVk round-trip" );

    Rhi::ClearValue depthClear{};
    depthClear.myType  = Rhi_ClearValueType::DepthStencil;
    depthClear.myDepth = 0.0f;

    Rhi::RenderPassBeginInfo begin{};
    begin.myRenderPass  = rp;
    begin.myFramebuffer = fb;
    begin.myWidth       = 64;
    begin.myHeight      = 64;
    begin.myClears      = &depthClear;
    begin.myClearCount  = 1;
    Rhi::CommandListBeginRenderPass( list, begin );
    Rhi::CommandListSetViewport( list, Rhi::Viewport{ 0.f, 0.f, 64.f, 64.f, 0.f, 1.f } );
    Rhi::CommandListSetScissor( list, Rhi::Scissor{ 0, 0, 64, 64 } );
    Rhi::CommandListSetDepthBias( list, 1.0f, 0.0f, 1.5f );

    const uint32_t dynOffset = 256u;
    Rhi::CommandListBindDescriptorSet( list, Rhi_PipelineBindPoint::Graphics, {}, 0, {}, &dynOffset, 1 );
    Rhi::CommandListBindVertexBuffer( list, 0, RhiVulkan::BufferAdopt( reinterpret_cast< VkBuffer >( static_cast< uintptr_t >( 0x33 ) ) ), 0 );
    Rhi::CommandListBindIndexBuffer( list, RhiVulkan::BufferAdopt( reinterpret_cast< VkBuffer >( static_cast< uintptr_t >( 0x44 ) ) ), 0, Rhi_IndexType::Uint32 );
    Rhi::CommandListDraw( list, 3, 1, 0, 0 );
    Rhi::CommandListDrawIndexed( list, 3, 1, 0, 0, 0 );
    Rhi::CommandListEndRenderPass( list );

    Rhi::MemoryBarrierDesc mem{};
    mem.mySrcStage  = Rhi_PipelineStage::ComputeShader;
    mem.myDstStage  = Rhi_PipelineStage::ComputeShader;
    mem.mySrcAccess = Rhi_Access::ShaderWrite;
    mem.myDstAccess = Rhi_Access::ShaderRead;
    Rhi::CommandListMemoryBarrier( list, mem );

    const Rhi_PipelineStage depthStages = Rhi_PipelineStage::EarlyFragmentTests | Rhi_PipelineStage::LateFragmentTests;
    Expect( ( static_cast< uint32_t >( depthStages ) & static_cast< uint32_t >( Rhi_PipelineStage::EarlyFragmentTests ) ) != 0, "EarlyFragmentTests flag set" );
    Expect( ( static_cast< uint32_t >( depthStages ) & static_cast< uint32_t >( Rhi_PipelineStage::LateFragmentTests ) ) != 0, "LateFragmentTests flag set" );

    Rhi::CommandListDestroy( list );
    Rhi::DeviceDestroy( device );
}

}  // namespace

int main() {
    const auto repoRoot = FindRepoRootFromExe();
    if ( repoRoot.empty() ) {
        std::cerr << "GfxTests: cannot find repo root (VulkanDesktop.sln)\n";
        return 1;
    }

    Util_EngineConfig config;
    try {
        InitTestConfig( repoRoot, config );
    }
    catch ( const std::exception& e ) {
        std::cerr << "GfxTests init failed: " << e.what() << '\n';
        return 1;
    }

    Gfx_SetMaterialTableGenerationForExtract( 1 );

    TestConfigPrecedence( repoRoot );
    TestSliceSceneAssets( repoRoot );
    TestSoAGeneration();
    TestTransparentSortStableUnderRotation();
    TestDirectionalLightViewOrientation();
    TestVulkanViewportClipZContract();
    TestDirectionalShadowSetupForSmallScene();
    TestSunElevationShadowGate();
    TestEntityGpuRecordSync();
    TestEntityIndirectSlot();
    TestCpuGpuCullParityDemoViews();
    TestCpuGpuCullParityLayerMask();
    TestLodGpuEntityRecordParity();
    TestClusterIndexFromTile();
    TestClusterGridCount();
    TestRenderPresetHybridDeferred();
    TestHybridDeferredFramePlan();
    TestResolveEnableFlagsFromBuildInput();
    TestTemporalHaltonJitter();
    TestRhiDeviceHeadlessConstruct();
    TestRhiOpaqueDeviceAndCommandList();
    TestRhiGraphicsRecordingSurface();
    TestGpuCullSkipsCpuFrustumCull();
    TestDemoCullAndBatch();

    // Gpu_MaterialTableEntry std430 layout: static_assert in Vk_Types.h (VulkanDesktop build). Shader: VK_MAX_BINDLESS_TEXTURES.
    Expect( VkDescriptorPolicy::kMaxBindlessTextures == 64, "kMaxBindlessTextures must match TriangleFrag_Lit_Bindless.frag VK_MAX_BINDLESS_TEXTURES" );

    if ( gFailures > 0 ) {
        std::cerr << "GfxTests: " << gFailures << " failure(s)\n";
        return 1;
    }

    std::cout << "GfxTests: all passed\n";
    return 0;
}
