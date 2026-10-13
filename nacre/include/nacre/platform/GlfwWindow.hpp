#pragma once

#include <string>
#include <vector>

#include <nacre/core/Logger.hpp>

struct GLFWwindow;

namespace nacre::platform {

class GlfwWindow {
    public:
    GlfwWindow(int width, int height, const std::string& title);
    ~GlfwWindow();

    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;

    GlfwWindow(GlfwWindow&&) = delete;
    GlfwWindow& operator=(GlfwWindow&&) = delete;

    void pollEvents();
    bool shouldClose() const;

    GLFWwindow* getNativeWindow() const;

    static std::vector<const char*> requiredInstanceExtensions();

    private:
    nacre::core::Logger logger{"GLFW Window"};

    GLFWwindow* window;
};

} // namespace nacre::platform
