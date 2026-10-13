#include <nacre/platform/GlfwWindow.hpp>

#include <cstdint>

#include <GLFW/glfw3.h>

namespace nacre::platform {

GlfwWindow::GlfwWindow(int width, int height, const std::string& title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        logger.fatal("Failed to create GLFW window");
    }
}

GlfwWindow::~GlfwWindow() {
    glfwDestroyWindow(window);
}

void GlfwWindow::pollEvents() {
    glfwPollEvents();
}

bool GlfwWindow::shouldClose() const {
    return glfwWindowShouldClose(window);
}

GLFWwindow* GlfwWindow::getNativeWindow() const {
    return window;
}

std::vector<const char*> GlfwWindow::requiredInstanceExtensions() {
    uint32_t count = 0;
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    if (names == nullptr) {
        core::Logger("GLFW Window").fatal("Vulkan is not supported on this system");
    }
    return {names, names + count};
}

} // namespace nacre::platform
