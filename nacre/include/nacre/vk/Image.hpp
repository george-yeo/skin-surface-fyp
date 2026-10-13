#pragma once

#include <vulkan/vulkan.h>

namespace nacre::vk {

class Image {
    public:
    Image(VkDevice device, VkImage image, VkFormat format);
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept;
    Image& operator=(Image&&) = delete;

    VkImage get() const { return image; }
    VkImageView getView() { return imageView; }

    private:
    VkDevice device;
    VkImage image;
    VkImageView imageView = VK_NULL_HANDLE;
};

} // namespace nacre::vk
