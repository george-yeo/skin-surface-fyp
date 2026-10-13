#pragma once

#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

// Takes ownership of a surface created by the windowing layer.
class Surface {
    public:
    Surface(const Instance& instance, VkSurfaceKHR handle);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    Surface(Surface&&) = delete;
    Surface& operator=(Surface&&) = delete;

    VkSurfaceKHR get() const { return surface; }

    private:
    core::Logger logger{"Vulkan Surface"};

    const Instance& instance;
    VkSurfaceKHR surface;
};

} // namespace nacre::vk
