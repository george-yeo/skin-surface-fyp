#pragma once

#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class Buffer {
    public:
    Buffer(Device& device, VkDeviceSize size,
           VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    Buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
           VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    // Persistently mapped: mapped once on first use, unmapped in destroy().
    void* map();

    void write(const void* data, VkDeviceSize writeSize = VK_WHOLE_SIZE);

    VkBuffer get() const { return buffer; }
    VkDeviceSize getSize() const { return size; }

    private:
    core::Logger logger{"Buffer"};

    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;

    uint32_t findMemoryType(VkPhysicalDevice physical, uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);

    void unmap();
    void destroy();
};

} // namespace nacre::vk
