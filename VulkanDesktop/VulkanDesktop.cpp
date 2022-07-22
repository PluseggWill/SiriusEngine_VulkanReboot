#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include "RenderCore/Vk_Core.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

const uint32_t WIDTH  = 1600;
const uint32_t HEIGHT = 1200;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif  // NDEBUG

const std::vector< const char* > validationLayers = { "VK_LAYER_KHRONOS_validation" };

const std::vector< const char* > deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

int main() {
    Vk_Core* app = &Vk_Core::GetInstance();
    app->SetSize( WIDTH, HEIGHT );
    app->SetEnableValidationLayers( enableValidationLayers, validationLayers );
    app->SetRequiredExtension( deviceExtensions );

    try {
        app->Run();
    }
    catch ( const std::exception& e ) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}