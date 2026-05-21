#include "Util_Input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace UtilInput {

void Sample( GLFWwindow* aWindow, Util_InputState& aState, Util_InputSnapshot& aOutSnapshot, bool aAllowKeyboard, bool aAllowMouseLook ) {
    aOutSnapshot = {};

    if ( aWindow == nullptr )
        return;

    if ( aAllowKeyboard ) {
        aOutSnapshot.myMoveForward = glfwGetKey( aWindow, GLFW_KEY_W ) == GLFW_PRESS;
        aOutSnapshot.myMoveBack    = glfwGetKey( aWindow, GLFW_KEY_S ) == GLFW_PRESS;
        aOutSnapshot.myMoveRight   = glfwGetKey( aWindow, GLFW_KEY_D ) == GLFW_PRESS;
        aOutSnapshot.myMoveLeft    = glfwGetKey( aWindow, GLFW_KEY_A ) == GLFW_PRESS;
        aOutSnapshot.myMoveUp      = glfwGetKey( aWindow, GLFW_KEY_E ) == GLFW_PRESS;
        aOutSnapshot.myMoveDown    = glfwGetKey( aWindow, GLFW_KEY_Q ) == GLFW_PRESS;
    }

    const bool wantMouseLook = aAllowMouseLook && glfwGetMouseButton( aWindow, GLFW_MOUSE_BUTTON_RIGHT ) == GLFW_PRESS;

    // RMB: capture cursor for look; first frame after capture skips delta to avoid a jump.
    if ( wantMouseLook && !aState.myMouseLookActive ) {
        aState.myMouseLookActive     = true;
        aState.myFirstMouseLookFrame = true;
        glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
    }
    else if ( !wantMouseLook && aState.myMouseLookActive ) {
        aState.myMouseLookActive = false;
        glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
    }

    if ( !wantMouseLook )
        return;

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos( aWindow, &cursorX, &cursorY );

    if ( aState.myFirstMouseLookFrame ) {
        aState.myLastCursorX         = static_cast< float >( cursorX );
        aState.myLastCursorY         = static_cast< float >( cursorY );
        aState.myFirstMouseLookFrame = false;
    }
    else {
        aOutSnapshot.myMouseDeltaX = static_cast< float >( cursorX ) - aState.myLastCursorX;
        aOutSnapshot.myMouseDeltaY = static_cast< float >( cursorY ) - aState.myLastCursorY;
    }

    aState.myLastCursorX = static_cast< float >( cursorX );
    aState.myLastCursorY = static_cast< float >( cursorY );
}

}  // namespace UtilInput
