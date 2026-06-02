// GfxTests: CPU-only regression tests for Gfx draw-stream (no Vulkan / GLFW).

#include "../Gfx/Gfx_DrawBatch.h"
#include "../Gfx/Gfx_DrawCullSort.h"
#include "../Gfx/Gfx_DrawExtract.h"
#include "../Gfx/Gfx_FrameDrawStream.h"
#include "../Gfx/Gfx_SceneSoA.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../Util/Util_EngineConfig.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdlib>
#include <filesystem>
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

void InitTestConfig( const std::filesystem::path& aRepoRoot ) {
    static std::vector< char > rootArgStorage;
    const std::string          rootStr = aRepoRoot.string();
    rootArgStorage.assign( rootStr.begin(), rootStr.end() );
    rootArgStorage.push_back( '\0' );

    static char arg0[] = "GfxTests";
    static char arg1[] = "--asset-root";
    char*       arg2   = rootArgStorage.data();

    char* argv[] = { arg0, arg1, arg2 };
    UtilEngineConfig::Initialize( 3, argv );
    Gfx_ShaderPermutation::Initialize();
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

Gfx_CullViewParams BuildDemoOverviewView() {
    const glm::vec3 eye{ 10.0f, 8.0f, 10.0f };
    const glm::vec3 center{ 0.0f, 0.0f, -4.0f };
    const glm::vec3 up{ 0.0f, 0.0f, 1.0f };
    const float     aspect = 1600.0f / 1200.0f;

    Gfx_CullViewParams view{};
    view.myView = glm::lookAt( eye, center, up );
    view.myProj = glm::perspective( glm::radians( 50.0f ), aspect, 0.1f, 200.0f );
    view.myProj[ 1 ][ 1 ] *= -1.0f;
    return view;
}

void TestSoAGeneration() {
    Gfx_SceneSoA scene;
    const auto    id1 = scene.AllocEntity( 0, 0, glm::mat4( 1.0f ) );
    Expect( scene.IsAlive( id1 ), "SoA: id1 alive" );

    Expect( scene.FreeEntity( id1 ), "SoA: free id1" );
    Expect( !scene.IsAlive( id1 ), "SoA: stale id1 not alive" );

    const auto id2 = scene.AllocEntity( 1, 1, glm::mat4( 1.0f ) );
    Expect( scene.IsAlive( id2 ), "SoA: id2 alive after slot reuse" );
    Expect( id2.myIndex == id1.myIndex, "SoA: reused slot index" );
    Expect( id2.myGeneration != id1.myGeneration, "SoA: generation bumped on reuse" );
}

void TestDemoCullAndBatch() {
    Gfx_SceneSoA               scene;
    PopulateDemoSceneSoA( scene );
    Expect( scene.GetActiveCount() == 9, "demo SoA entity count" );

    Gfx_LodTable               lodTable;
    Gfx_LodState               lodState;
    Gfx_FrameDrawStreamParams  params{};
    params.myScene       = &scene;
    params.myView        = BuildDemoOverviewView();
    params.myCameraEye   = glm::vec3( 10.0f, 8.0f, 10.0f );
    params.myCameraView  = params.myView.myView;
    params.myLodTable    = &lodTable;
    params.myLodState    = &lodState;

    Gfx_FrameDrawStreamOutput out;
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

    try {
        InitTestConfig( repoRoot );
    }
    catch ( const std::exception& e ) {
        std::cerr << "GfxTests init failed: " << e.what() << '\n';
        return 1;
    }

    Gfx_SetMaterialTableGenerationForExtract( 1 );

    TestSoAGeneration();
    TestDemoCullAndBatch();

    if ( gFailures > 0 ) {
        std::cerr << "GfxTests: " << gFailures << " failure(s)\n";
        return 1;
    }

    std::cout << "GfxTests: all passed\n";
    return 0;
}
