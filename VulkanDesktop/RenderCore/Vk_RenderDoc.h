#pragma once

#include <vulkan/vulkan.h>

#if defined( WIN32 ) || defined( __WIN32__ ) || defined( _WIN32 ) || defined( _MSC_VER )
#define RENDERDOC_CC __cdecl
#else
#define RENDERDOC_CC
#endif

// RenderDoc + VK_EXT_debug_utils bridge.
// Keeps capture and command-label plumbing outside Vk_Renderer orchestration code.
class Vk_RenderDoc {
public:
    void Configure( bool aEnableRenderDoc );
    bool IsEnabled() const;

    // True when we should request VK_EXT_debug_utils at instance creation.
    bool WantsDebugUtilsExtension() const;

    // Called before Vulkan instance/device init; attaches RenderDoc API if available.
    void InitRuntime();
    // Called after VkDevice creation; loads VK_EXT_debug_utils command-label entry points.
    void BindVulkanHandles( VkDevice aDevice );
    void Shutdown();

    void TriggerCaptureRequest();
    void BeginFrameCaptureIfRequested();

    void CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const;
    void CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const;

    // True when VK_EXT_debug_utils command-label entry points are loaded (--renderdoc + extension present).
    bool AreCommandLabelsEnabled() const;

private:
    struct Api {
        void( RENDERDOC_CC* GetAPIVersion )( int* major, int* minor, int* patch ) = nullptr;
        void( RENDERDOC_CC* SetCaptureKeys )( int* keys, int num )                = nullptr;
        void( RENDERDOC_CC* MaskOverlayBits )( uint32_t And, uint32_t Or )        = nullptr;
        void( RENDERDOC_CC* TriggerCapture )( void )                              = nullptr;
    };

    void AttachRenderDocApi();
    void LoadDebugLabelFunctions();

    bool                             myRenderDocEnabled        = false;
    bool                             myRenderDocApiReady       = false;
    bool                             myRenderDocCapturePending = false;
    Api                              myApi{};
    PFN_vkCmdBeginDebugUtilsLabelEXT myVkCmdBeginDebugUtilsLabel = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT   myVkCmdEndDebugUtilsLabel   = nullptr;
    VkDevice                         myDevice                    = VK_NULL_HANDLE;
};
