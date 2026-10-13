#include "skin/CameraController.hpp"

#include <GLFW/glfw3.h>

#include <nacre/platform/GlfwWindow.hpp>
#include <nacre/platform/InputManager.hpp>
#include <nacre/scene/Camera.hpp>

namespace skin {

CameraController::CameraController(nacre::platform::InputManager& input,
                                   nacre::platform::GlfwWindow& window,
                                   nacre::scene::Camera& camera)
    : window(window), camera(camera) {
    bindInputs(input);
}

void CameraController::bindInputs(nacre::platform::InputManager& input) {
    constexpr float speed = 10.0f;

    // camera movement
    bindings.push_back(input.bindKey(
        "W",
        [this](double dt) {
            camera.translate(glm::vec3(0.0f, 0.0f, -speed) * static_cast<float>(dt));
        },
        true));
    bindings.push_back(input.bindKey(
        "A",
        [this](double dt) {
            camera.translate(glm::vec3(-speed, 0.0f, 0.0f) * static_cast<float>(dt));
        },
        true));
    bindings.push_back(input.bindKey(
        "S",
        [this](double dt) {
            camera.translate(glm::vec3(0.0f, 0.0f, speed) * static_cast<float>(dt));
        },
        true));
    bindings.push_back(input.bindKey(
        "D",
        [this](double dt) {
            camera.translate(glm::vec3(speed, 0.0f, 0.0f) * static_cast<float>(dt));
        },
        true));
    bindings.push_back(input.bindKey(
        "E",
        [this](double dt) {
            camera.translate(glm::vec3(0.0f, speed, 0.0f) * static_cast<float>(dt));
        },
        true));
    bindings.push_back(input.bindKey(
        "Q",
        [this](double dt) {
            camera.translate(glm::vec3(0.0f, -speed, 0.0f) * static_cast<float>(dt));
        },
        true));

    bindings.push_back(input.bindKey("R", [this](double /*dt*/) { camera.resetPose(); }));

    // Right-drag mouse-look: cursor hidden and unbounded while held, restored
    // to where it was on release.
    constexpr float lookSensitivity = 0.15f; // degrees per pixel
    bindings.push_back(input.bindMouseDrag(
        GLFW_MOUSE_BUTTON_RIGHT,
        [this](double, double) {
            glfwSetInputMode(window.getNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(window.getNativeWindow(), &lastCursor.x, &lastCursor.y);
        },
        [this, lookSensitivity](double x, double y) {
            const glm::dvec2 delta = glm::dvec2(x, y) - lastCursor;
            lastCursor = {x, y};
            camera.rotate(glm::vec3(-delta.y, -delta.x, 0.0) * lookSensitivity);
        },
        [this]() {
            glfwSetInputMode(window.getNativeWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }));
}

} // namespace skin
