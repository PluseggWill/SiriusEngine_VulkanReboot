#include "Vk_RenderDoc.h"

#include "../Util/Util_Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <string>

namespace {
using pRENDERDOC_GetAPI = int( RENDERDOC_CC* )( int version, void** outApi );

struct RenderDocApiV170 {
    void( RENDERDOC_CC* GetAPIVersion )( int*, int*, int* );
    void* SetCaptureOptionU32;
    void* SetCaptureOptionF32;
    void* GetCaptureOptionU32;
    void* GetCaptureOptionF32;
    void* SetFocusToggleKeys;
    void( RENDERDOC_CC* SetCaptureKeys )( int*, int );
    uint32_t( RENDERDOC_CC* GetOverlayBits )( void );
    void( RENDERDOC_CC* MaskOverlayBits )( uint32_t, uint32_t );
    void* RemoveHooksUnion0;
    void* UnloadCrashHandler;
    void* SetCaptureFilePathTemplateUnion0;
    void* GetCaptureFilePathTemplateUnion0;
    void* GetNumCaptures;
    void* GetCapture;
    void( RENDERDOC_CC* TriggerCapture )( void );
    void* IsTargetControlConnectedUnion0;
    void* LaunchReplayUI;
    void( RENDERDOC_CC* SetActiveWindow )( void*, void* );
    void( RENDERDOC_CC* StartFrameCapture )( void*, void* );
    uint32_t( RENDERDOC_CC* IsFrameCapturing )( void );
    uint32_t( RENDERDOC_CC* EndFrameCapture )( void*, void* );
};
}  // namespace

void Vk_RenderDoc::Configure( bool aEnableRenderDoc ) {
    myRenderDocEnabled = aEnableRenderDoc;
}

bool Vk_RenderDoc::IsEnabled() const {
    return myRenderDocEnabled;
}

bool Vk_RenderDoc::WantsDebugUtilsExtension() const {
    return myRenderDocEnabled;
}

void Vk_RenderDoc::InitRuntime() {
    AttachRenderDocApi();
}

void Vk_RenderDoc::BindVulkanHandles( VkDevice aDevice ) {
    myDevice = aDevice;
    LoadDebugLabelFunctions();
}

void Vk_RenderDoc::Shutdown() {
    myRenderDocApiReady         = false;
    myRenderDocCapturePending   = false;
    myApi                       = {};
    myVkCmdBeginDebugUtilsLabel = nullptr;
    myVkCmdEndDebugUtilsLabel   = nullptr;
    myDevice                    = VK_NULL_HANDLE;
}

void Vk_RenderDoc::AttachRenderDocApi() {
    if ( !myRenderDocEnabled ) {
        return;
    }

    // Passive mode only: app never calls LoadLibrary. RenderDoc must inject itself
    // (typically when launched from RenderDoc UI) before this point.
    HMODULE renderDocModule = GetModuleHandleA( "renderdoc.dll" );
    if ( renderDocModule == nullptr ) {
        UtilLogger::Warn( "RENDERDOC", "Requested by --renderdoc, but renderdoc.dll is not injected/loaded in this process." );
        return;
    }

    const auto getApi = reinterpret_cast< pRENDERDOC_GetAPI >( GetProcAddress( renderDocModule, "RENDERDOC_GetAPI" ) );
    if ( getApi == nullptr ) {
        UtilLogger::Warn( "RENDERDOC", "renderdoc.dll found, but RENDERDOC_GetAPI is missing." );
        return;
    }

    void*     api               = nullptr;
    int       negotiatedVersion = 0;
    const int kApiCandidates[]  = { 10700, 10600, 10500, 10402, 10401, 10400, 10300, 10200, 10102, 10101, 10100, 10002, 10001, 10000 };
    for ( int version : kApiCandidates ) {
        api = nullptr;
        if ( getApi( version, &api ) != 0 && api != nullptr ) {
            negotiatedVersion = version;
            break;
        }
    }
    if ( negotiatedVersion == 0 ) {
        UtilLogger::Warn( "RENDERDOC", "RENDERDOC_GetAPI failed for all supported API versions." );
        return;
    }

    myRenderDocApiReady   = true;
    const auto* apiV170   = reinterpret_cast< const RenderDocApiV170* >( api );
    myApi.GetAPIVersion   = apiV170->GetAPIVersion;
    myApi.SetCaptureKeys  = apiV170->SetCaptureKeys;
    myApi.MaskOverlayBits = apiV170->MaskOverlayBits;
    myApi.TriggerCapture  = apiV170->TriggerCapture;

    if ( myApi.GetAPIVersion != nullptr ) {
        int major = 0, minor = 0, patch = 0;
        myApi.GetAPIVersion( &major, &minor, &patch );
        UtilLogger::Info( "RENDERDOC", "RenderDoc API version " + std::to_string( major ) + "." + std::to_string( minor ) + "." + std::to_string( patch ) + " (requested "
                                           + std::to_string( negotiatedVersion ) + ")" );
    }
    if ( myApi.MaskOverlayBits != nullptr ) {
        constexpr uint32_t kOverlayAll = 0x07ffffffu;
        myApi.MaskOverlayBits( 0xffffffffu, kOverlayAll );
    }
    if ( myApi.SetCaptureKeys != nullptr ) {
        myApi.SetCaptureKeys( nullptr, 0 );
    }
    UtilLogger::Info( "RENDERDOC", "RenderDoc API attached (startup-gated by --renderdoc)." );
}

void Vk_RenderDoc::LoadDebugLabelFunctions() {
    if ( myDevice == VK_NULL_HANDLE ) {
        return;
    }
    myVkCmdBeginDebugUtilsLabel = reinterpret_cast< PFN_vkCmdBeginDebugUtilsLabelEXT >( vkGetDeviceProcAddr( myDevice, "vkCmdBeginDebugUtilsLabelEXT" ) );
    myVkCmdEndDebugUtilsLabel   = reinterpret_cast< PFN_vkCmdEndDebugUtilsLabelEXT >( vkGetDeviceProcAddr( myDevice, "vkCmdEndDebugUtilsLabelEXT" ) );
    if ( myVkCmdBeginDebugUtilsLabel != nullptr && myVkCmdEndDebugUtilsLabel != nullptr ) {
        UtilLogger::Info( "RENDER", "VK_EXT_debug_utils command labels enabled." );
    }
    else {
        UtilLogger::Warn( "RENDER", "VK_EXT_debug_utils command labels unavailable; RenderDoc draw tags disabled." );
    }
}

void Vk_RenderDoc::TriggerCaptureRequest() {
    if ( !myRenderDocEnabled ) {
        return;
    }
    myRenderDocCapturePending = true;
    UtilLogger::Info( "RENDERDOC", "F12 capture requested; scheduling next-frame capture." );
}

void Vk_RenderDoc::BeginFrameCaptureIfRequested() {
    if ( !myRenderDocCapturePending || !myRenderDocApiReady ) {
        return;
    }
    // TriggerCapture is the stable path for injected RenderDoc sessions.
    // We intentionally do not call StartFrameCapture/EndFrameCapture manually.
    if ( myApi.TriggerCapture != nullptr ) {
        myApi.TriggerCapture();
        UtilLogger::Info( "RENDERDOC", "TriggerCapture issued for next frame." );
    }
    else {
        UtilLogger::Warn( "RENDERDOC", "Capture requested but no RenderDoc capture entry points available." );
    }
    myRenderDocCapturePending = false;
}

void Vk_RenderDoc::CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const {
    if ( myVkCmdBeginDebugUtilsLabel == nullptr || aLabelName == nullptr || aLabelName[ 0 ] == '\0' ) {
        return;
    }
    VkDebugUtilsLabelEXT label{};
    label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = aLabelName;
    label.color[ 0 ] = 0.20f;
    label.color[ 1 ] = 0.75f;
    label.color[ 2 ] = 1.00f;
    label.color[ 3 ] = 1.00f;
    myVkCmdBeginDebugUtilsLabel( aCommandBuffer, &label );
}

void Vk_RenderDoc::CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const {
    if ( myVkCmdEndDebugUtilsLabel == nullptr ) {
        return;
    }
    myVkCmdEndDebugUtilsLabel( aCommandBuffer );
}
