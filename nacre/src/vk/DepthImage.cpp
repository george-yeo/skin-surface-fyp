#include <nacre/vk/DepthImage.hpp>
#include <nacre/vk/Device.hpp>

namespace nacre::vk {

DepthImage::DepthImage(Device& device, VkFormat format, VkExtent2D extent) : device(device) {
    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.pNext = nullptr;

    if (vkCreateImage(device.get(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
        logger.fatal("Failed to create depth image");
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device.get(), image, &memRequirements);

    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(device.getPhysicalDevice(), &memoryProperties);

    uint32_t memoryType = UINT32_MAX;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            memoryType = i;
            break;
        }
    }

    if (memoryType == UINT32_MAX) {
        vkDestroyImage(device.get(), image, nullptr);
        logger.fatal("Failed to find suitable image memory type");
    }

    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;

    if (vkAllocateMemory(device.get(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device.get(), image, nullptr);
        logger.fatal("Failed to allocate image memory");
    }

    // Bind memory
    if (vkBindImageMemory(device.get(), image, memory, 0) != VK_SUCCESS) {
        vkFreeMemory(device.get(), memory, nullptr);
        vkDestroyImage(device.get(), image, nullptr);
        logger.fatal("Failed to bind image memory");
    }

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.get(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        vkFreeMemory(device.get(), memory, nullptr);
        vkDestroyImage(device.get(), image, nullptr);

        logger.fatal("Failed to create depth image view");
    }
}

DepthImage::~DepthImage() {
    vkDestroyImageView(device.get(), imageView, nullptr);
    vkFreeMemory(device.get(), memory, nullptr);
    vkDestroyImage(device.get(), image, nullptr);
}

DepthImage::DepthImage(DepthImage&& other) noexcept
    : device(other.device),
      image(other.image),
      imageView(other.imageView),
      memory(other.memory),
      format(other.format) {
    other.image = VK_NULL_HANDLE;
    other.imageView = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
}

} // namespace nacre::vk
