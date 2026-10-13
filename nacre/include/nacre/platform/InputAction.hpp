#pragma once

#include <functional>
#include <string>

namespace nacre::platform {

struct InputAction {
    std::string name;
    int keyCode;
    bool isPressed = false;
    bool canHold = false;

    std::function<void(double)> onAction;
};

} // namespace nacre::platform
