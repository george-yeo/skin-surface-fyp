#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <nacre/platform/InputBinding.hpp>

#include <nacre/platform/fwd.hpp>
#include <nacre/scene/fwd.hpp>

namespace skin {

// WASD/QE fly, R resets the pose, right-drag mouse-look.
class CameraController {
    public:
    CameraController(nacre::platform::InputManager& input, nacre::platform::GlfwWindow& window,
                     nacre::scene::Camera& camera);

    // The input bindings capture `this`.
    CameraController(const CameraController&) = delete;
    CameraController& operator=(const CameraController&) = delete;
    CameraController(CameraController&&) = delete;
    CameraController& operator=(CameraController&&) = delete;

    private:
    nacre::platform::GlfwWindow& window;
    nacre::scene::Camera& camera;
    glm::dvec2 lastCursor{0.0};

    // Declared last: unbinds before anything the callbacks use is destroyed.
    std::vector<nacre::platform::InputBinding> bindings;

    void bindInputs(nacre::platform::InputManager& input);
};

} // namespace skin
