#pragma once

#include "Util_InputSnapshot.h"

struct GLFWwindow;

// GLFW keyboard + RMB fly look. RMB uses GLFW_CURSOR_DISABLED with per-frame cursor recenter (virtual-center delta).
namespace UtilInput {
void Sample( GLFWwindow* aWindow, Util_InputState& aState, Util_InputSnapshot& aOutSnapshot, bool aAllowKeyboard, bool aAllowMouseLook );
}  // namespace UtilInput
