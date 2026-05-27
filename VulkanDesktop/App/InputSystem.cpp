#include "InputSystem.h"

#include "../Util/Util_Input.h"

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

    UtilInput::Sample( aWindow, myState, mySnapshot, allowKeyboard, allowMouse );
    myLastSampleTime    = std::chrono::high_resolution_clock::now();
    myHasLastSampleTime = true;
}
