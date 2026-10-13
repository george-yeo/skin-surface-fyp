#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class Swapchain {
    public:
    Swapchain(Device& device, VkSurfaceKHR surface, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    VkSwapchainKHR get() const;

    const std::vector<Image>& getImages() const;
    std::vector<Image>& getImages();
    const std::vector<DepthImage>& getDepthImages() const;
    std::vector<DepthImage>& getDepthImages();
    VkFormat getFormat() const;
    VkFormat getDepthFormat() const;
    size_t getImageCount() const;
    VkExtent2D getExtent() const;
    // Signalled by the submit that renders image `imageIndex`, waited on by its present.
    VkSemaphore getRenderFinishedSemaphore(uint32_t imageIndex) const;

    private:
    // pImpl to hide Vulkan-heavy members
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace nacre::vk
