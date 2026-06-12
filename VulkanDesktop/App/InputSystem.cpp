#include "InputSystem.h"

#include "../Util/Util_Input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

void InputSystem::Sample( GLFWwindow* aWindow ) {
    mySnapshot = {};

    if ( aWindow == nullptr ) {
        return;
    }

    bool allowKeyboard = true;
    bool allowMouse    = true;
    if ( ImGui::GetCurrentContext() != nullptr ) {
        const ImGuiIO& io = ImGui::GetIO();
        allowKeyboard     = !io.WantCaptureKeyboard;
        allowMouse        = !io.WantCaptureMouse;
    }

    // Fly camera: RMB look bypasses ImGui capture (see InputSystem::Sample).
    const bool rmbLook = glfwGetMouseButton( aWindow, GLFW_MOUSE_BUTTON_RIGHT ) == GLFW_PRESS;
    if ( rmbLook ) {
        allowKeyboard = true;
        allowMouse    = true;
    }

    UtilInput::Sample( aWindow, myState, mySnapshot, allowKeyboard, allowMouse );
    myLastSampleTime    = std::chrono::high_resolution_clock::now();
    myHasLastSampleTime = true;
}
