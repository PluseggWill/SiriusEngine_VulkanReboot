// GfxTests: CPU-only regression tests for Gfx draw-stream (no Vulkan / GLFW).

#include "../Gfx/Gfx_DrawBatch.h"
#include "../Gfx/Gfx_DrawCullSort.h"
#include "../Gfx/Gfx_DrawExtract.h"
#include "../Gfx/Gfx_EntityGpuRecord.h"
#include "../Gfx/Gfx_FrameDrawStream.h"
#include "../Gfx/Gfx_GpuCull.h"
#include "../Gfx/Gfx_Lod.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_SceneSoA.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../RenderCore/Vk_DescriptorPolicy.h"
#include "../Util/Util_EngineConfig.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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
    Gfx_ShaderPermutation::Initialize( aOutConfig );
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

void TestEntityGpuRecordSync() {
    Gfx_SceneSoA scene;

    const glm::mat4 worldTransform = glm::translate( glm::mat4( 1.0f ), glm::vec3( 1.0f, 0.0f, 0.0f ) );
    const auto      id             = scene.AllocEntity( 2, 3, worldTransform );

    Gfx_Bounds localBounds{};
    localBounds.myMin = glm::vec3( -1.0f );
    localBounds.myMax = glm::vec3( 1.0f );
    scene.SetLocalBoundsForSlot( id.myIndex, localBounds );

    Gfx_EntityGpuRecord record{};
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
    TestSoAGeneration();
    TestTransparentSortStableUnderRotation();
    TestEntityGpuRecordSync();
    TestEntityIndirectSlot();
    TestCpuGpuCullParityDemoViews();
    TestCpuGpuCullParityLayerMask();
    TestLodGpuEntityRecordParity();
    TestGpuCullSkipsCpuFrustumCull();
    TestDemoCullAndBatch();

    // GpuMaterialTableEntry std430 layout: static_assert in Vk_Types.h (VulkanDesktop build). Shader: VK_MAX_BINDLESS_TEXTURES.
    Expect( VkDescriptorPolicy::kMaxBindlessTextures == 64, "kMaxBindlessTextures must match TriangleFrag_Lit_Bindless.frag VK_MAX_BINDLESS_TEXTURES" );

    if ( gFailures > 0 ) {
        std::cerr << "GfxTests: " << gFailures << " failure(s)\n";
        return 1;
    }

    std::cout << "GfxTests: all passed\n";
    return 0;
}
