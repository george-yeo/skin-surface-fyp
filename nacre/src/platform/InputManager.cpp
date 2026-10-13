#include <nacre/platform/GlfwWindow.hpp>
#include <nacre/platform/InputManager.hpp>

#include <utility>

namespace nacre::platform {

GLFWKeyCode InputManager::parseStringToGLFWKey(const std::string& str) {
    static const std::unordered_map<std::string, GLFWKeyCode> keyMap = {
        {"W", GLFW_KEY_W},
        {"A", GLFW_KEY_A},
        {"S", GLFW_KEY_S},
        {"D", GLFW_KEY_D},
        {"Q", GLFW_KEY_Q},
        {"E", GLFW_KEY_E},
        {"P", GLFW_KEY_P},
        {"R", GLFW_KEY_R},
        {"LeftArrow", GLFW_KEY_LEFT},
        {"RightArrow", GLFW_KEY_RIGHT},
        {"UpArrow", GLFW_KEY_UP},
        {"DownArrow", GLFW_KEY_DOWN}};

    auto it = keyMap.find(str);
    if (it != keyMap.end()) {
        return it->second;
    }
    return GLFW_KEY_UNKNOWN;
}

InputBinding::~InputBinding() {
    release();
}

InputBinding::InputBinding(InputBinding&& other) noexcept : manager(other.manager), id(other.id) {
    other.manager = nullptr;
}

InputBinding& InputBinding::operator=(InputBinding&& other) noexcept {
    if (this != &other) {
        release();
        manager = other.manager;
        id = other.id;
        other.manager = nullptr;
    }
    return *this;
}

void InputBinding::release() {
    if (manager != nullptr) {
        manager->unbind(id);
        manager = nullptr;
    }
}

InputBinding InputManager::bindKey(const std::string& key, std::function<void(double)> action,
                                   bool canHold) {
    const GLFWKeyCode keyCode = parseStringToGLFWKey(key);
    if (keyCode == GLFW_KEY_UNKNOWN) {
        // Unknown names would all share the GLFW_KEY_UNKNOWN slot.
        logger.error("bindKey: unknown key name '{}', binding ignored", key);
        return {};
    }
    if (keyBindings.contains(keyCode)) {
        logger.error("bindKey: '{}' is already bound, binding ignored", key);
        return {};
    }

    InputAction inputAction{};
    inputAction.name = key;
    inputAction.keyCode = keyCode;
    inputAction.onAction = action;
    inputAction.canHold = canHold;
    keyBindings[inputAction.keyCode] = inputAction;

    const uint64_t id = nextBindingId++;
    keyBindingIds[id] = keyCode;
    return InputBinding{this, id};
}

InputBinding InputManager::bindMouseDrag(int glfwButton,
                                         std::function<void(double, double)> onPress,
                                         std::function<void(double, double)> onDrag,
                                         std::function<void()> onRelease) {
    const uint64_t id = nextBindingId++;
    mouseDragBindings.push_back({.button = glfwButton,
                                 .onPress = std::move(onPress),
                                 .onDrag = std::move(onDrag),
                                 .onRelease = std::move(onRelease),
                                 .id = id});
    return InputBinding{this, id};
}

void InputManager::unbind(uint64_t id) {
    if (updating) {
        pendingUnbinds.push_back(id);
    } else {
        removeBinding(id);
    }
}

void InputManager::removeBinding(uint64_t id) {
    if (auto it = keyBindingIds.find(id); it != keyBindingIds.end()) {
        keyBindings.erase(it->second);
        keyBindingIds.erase(it);
        return;
    }
    std::erase_if(mouseDragBindings, [id](const MouseDragBinding& binding) { return binding.id == id; });
}

InputAction& InputManager::getInputAction(const std::string& name) {
    return keyBindings.at(parseStringToGLFWKey(name));
}

void InputManager::update(double dt) {
    updating = true;

    std::unordered_map<GLFWKeyCode, bool> currentKeyStates;
    for (const auto& [key, action] : keyBindings) {
        if (glfwGetKey(window.getNativeWindow(), key) == GLFW_PRESS) {
            currentKeyStates[key] = true;
        } else if (glfwGetKey(window.getNativeWindow(), key) == GLFW_RELEASE) {
            currentKeyStates[key] = false;
        }
    }

    for (const auto& [key, isPressed] : currentKeyStates) {
        if (!keyBindings[key].canHold) {
            if (keyBindings[key].isPressed != isPressed && isPressed) {
                keyBindings[key].onAction(dt);
            }
        } else {
            if (isPressed) {
                keyBindings[key].onAction(dt);
            }
        }
        keyBindings[key].isPressed = isPressed;
    }

    GLFWwindow* native = window.getNativeWindow();
    for (auto& binding : mouseDragBindings) {
        const bool pressed = glfwGetMouseButton(native, binding.button) == GLFW_PRESS;
        if (pressed) {
            double x, y;
            glfwGetCursorPos(native, &x, &y);
            if (binding.held) {
                binding.onDrag(x, y);
            } else {
                binding.onPress(x, y);
            }
        } else if (binding.held) {
            binding.onRelease();
        }
        binding.held = pressed;
    }

    updating = false;
    for (uint64_t id : pendingUnbinds) {
        removeBinding(id);
    }
    pendingUnbinds.clear();
}

} // namespace nacre::platform
