#pragma once

#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class DepthImage {
    public:
    DepthImage(Device& device, VkFormat format, VkExtent2D extent); // todo: clean up
    ~DepthImage();

    DepthImage(const DepthImage&) = delete;
    DepthImage& operator=(const DepthImage&) = delete;

    DepthImage(DepthImage&& other) noexcept;
    DepthImage& operator=(DepthImage&&) = delete;

    VkFormat getFormat() const { return format; }
    VkImage get() const { return image; }
    VkImageView getView() const { return imageView; }

    private:
    core::Logger logger{"DepthImage"};

    Device& device;
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory memory;
    VkFormat format;
};

} // namespace nacre::vk
