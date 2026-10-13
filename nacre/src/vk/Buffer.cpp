#include <nacre/vk/Buffer.hpp>
#include <nacre/vk/Check.hpp>

#include <cstring>

#include <nacre/vk/Device.hpp>

namespace nacre::vk {

Buffer::Buffer(Device& device, VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties)
    : Buffer(device.get(), device.getPhysicalDevice(), size, usage, properties) {}

Buffer::Buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
       VkMemoryPropertyFlags properties)
    : device(device), physicalDevice(physicalDevice), size(size) {
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &createInfo, nullptr, &buffer) != VK_SUCCESS) {
        logger.fatal("Failed to create");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        logger.fatal("Failed to allocate memory");
    }

    vk::check(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
}

Buffer::~Buffer() {
    destroy();
}

void Buffer::destroy() {
    unmap();
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, buffer, nullptr);
    if (memory != VK_NULL_HANDLE)
        vkFreeMemory(device, memory, nullptr);
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
}

uint32_t Buffer::findMemoryType(VkPhysicalDevice physical, uint32_t typeFilter,
                               VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    logger.fatal("Failed to find suitable memory type ");
}

Buffer::Buffer(Buffer&& other) noexcept
    : device(other.device),
      physicalDevice(other.physicalDevice),
      buffer(other.buffer),
      memory(other.memory),
      size(other.size),
      mapped(other.mapped) {
    other.buffer = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.mapped = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        destroy();
        device = other.device;
        physicalDevice = other.physicalDevice;
        buffer = other.buffer;
        memory = other.memory;
        size = other.size;
        mapped = other.mapped;
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.mapped = nullptr;
    }
    return *this;
}

void* Buffer::map() {
    if (mapped == nullptr && vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS) {
        logger.fatal("Failed to map memory");
    }
    return mapped;
}

void Buffer::unmap() {
    if (mapped != nullptr) {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
}

void Buffer::write(const void* data, VkDeviceSize writeSize) {
    memcpy(map(), data, writeSize == VK_WHOLE_SIZE ? size : writeSize);
}

} // namespace nacre::vk
