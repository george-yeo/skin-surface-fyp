#include <nacre/vk/Surface.hpp>

#include <nacre/vk/Instance.hpp>

namespace nacre::vk {

Surface::Surface(const Instance& instance, VkSurfaceKHR handle)
    : instance(instance), surface(handle) {
    logger.print("Vulkan surface created");
}

Surface::~Surface() {
    vkDestroySurfaceKHR(instance.get(), surface, nullptr);
    logger.print("Vulkan surface destroyed");
}

} // namespace nacre::vk
