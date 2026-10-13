#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>

#include <nacre/core/Logger.hpp>
#include <nacre/platform/InputAction.hpp>
#include <nacre/platform/InputBinding.hpp>
#include <nacre/platform/fwd.hpp>

namespace nacre::platform {

using GLFWKeyCode = int;

class InputManager {
    public:
    explicit InputManager(GlfwWindow& window) : window(window) {}
    ~InputManager() = default;

    // Empty binding (and an error) if the key name is unknown or already bound.
    InputBinding bindKey(const std::string& key, std::function<void(double)> action,
                         bool canHold = false);

    // onPress fires once when `glfwButton` goes down, onDrag every update while
    // it stays down (both get the cursor position), onRelease once on release.
    InputBinding bindMouseDrag(int glfwButton, std::function<void(double, double)> onPress,
                               std::function<void(double, double)> onDrag,
                               std::function<void()> onRelease);

    void update(double dt);

    InputAction& getInputAction(const std::string& name);

    private:
    core::Logger logger{"InputManager"};

    friend class InputBinding;

    GlfwWindow& window;
    std::unordered_map<GLFWKeyCode, InputAction> keyBindings;
    std::unordered_map<uint64_t, GLFWKeyCode> keyBindingIds;
    uint64_t nextBindingId = 1;

    // A callback may destroy a binding mid-update; removal waits until the
    // loops are done so they never iterate a container that changed under them.
    bool updating = false;
    std::vector<uint64_t> pendingUnbinds;

    struct MouseDragBinding {
        int button;
        std::function<void(double, double)> onPress;
        std::function<void(double, double)> onDrag;
        std::function<void()> onRelease;
        bool held = false;
        uint64_t id = 0;
    };
    std::vector<MouseDragBinding> mouseDragBindings;

    GLFWKeyCode parseStringToGLFWKey(const std::string& str);
    void unbind(uint64_t id);
    void removeBinding(uint64_t id);
};

} // namespace nacre::platform
