#pragma once

#include "Util_InputSnapshot.h"

struct GLFWwindow;

namespace UtilInput {
void Sample( GLFWwindow* aWindow, Util_InputState& aState, Util_InputSnapshot& aOutSnapshot, bool aAllowKeyboard, bool aAllowMouseLook );
}  // namespace UtilInput
