#include "Util_Input.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {

void SetRawMouseMotionEnabled( GLFWwindow* aWindow, bool aEnabled ) {
#ifdef GLFW_RAW_MOUSE_MOTION
    if ( glfwRawMouseMotionSupported() ) {
        glfwSetInputMode( aWindow, GLFW_RAW_MOUSE_MOTION, aEnabled ? GLFW_TRUE : GLFW_FALSE );
    }
#else
    ( void )aWindow;
    ( void )aEnabled;
#endif
}

}  // namespace

namespace UtilInput {

void Sample( GLFWwindow* aWindow, Util_InputState& aState, Util_InputSnapshot& aOutSnapshot, bool aAllowKeyboard, bool aAllowMouseLook ) {
    aOutSnapshot = {};

    if ( aWindow == nullptr ) {
        return;
    }

    if ( aAllowKeyboard ) {
        aOutSnapshot.myMoveForward = glfwGetKey( aWindow, GLFW_KEY_W ) == GLFW_PRESS;
        aOutSnapshot.myMoveBack    = glfwGetKey( aWindow, GLFW_KEY_S ) == GLFW_PRESS;
        aOutSnapshot.myMoveRight   = glfwGetKey( aWindow, GLFW_KEY_D ) == GLFW_PRESS;
        aOutSnapshot.myMoveLeft    = glfwGetKey( aWindow, GLFW_KEY_A ) == GLFW_PRESS;
        aOutSnapshot.myMoveUp      = glfwGetKey( aWindow, GLFW_KEY_E ) == GLFW_PRESS;
        aOutSnapshot.myMoveDown    = glfwGetKey( aWindow, GLFW_KEY_Q ) == GLFW_PRESS;
    }

    const bool wantMouseLook = aAllowMouseLook && glfwGetMouseButton( aWindow, GLFW_MOUSE_BUTTON_RIGHT ) == GLFW_PRESS;

    if ( wantMouseLook && !aState.myMouseLookActive ) {
        aState.myMouseLookActive     = true;
        aState.myFirstMouseLookFrame = true;
        glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
        SetRawMouseMotionEnabled( aWindow, true );
    }
    else if ( !wantMouseLook && aState.myMouseLookActive ) {
        aState.myMouseLookActive = false;
        SetRawMouseMotionEnabled( aWindow, false );
        glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
    }

    if ( !wantMouseLook ) {
        return;
    }

    int winW = 0;
    int winH = 0;
    glfwGetWindowSize( aWindow, &winW, &winH );
    if ( winW <= 0 || winH <= 0 ) {
        return;
    }

    // Cursor position is in window coordinates; recenter each frame so delta stays stable with CURSOR_DISABLED.
    const double centerX = static_cast< double >( winW ) * 0.5;
    const double centerY = static_cast< double >( winH ) * 0.5;

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos( aWindow, &cursorX, &cursorY );

    if ( !aState.myFirstMouseLookFrame ) {
        aOutSnapshot.myMouseDeltaX = static_cast< float >( cursorX - centerX );
        aOutSnapshot.myMouseDeltaY = static_cast< float >( cursorY - centerY );
    }
    aState.myFirstMouseLookFrame = false;

    glfwSetCursorPos( aWindow, centerX, centerY );
}

}  // namespace UtilInput
