#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/Check.hpp>

#include <nacre/vk/CommandPool.hpp>
#include <nacre/vk/Device.hpp>

namespace nacre::vk {

CommandBuffer::CommandBuffer(Device& device, CommandPool& commandPool)
    : device(device), pool(commandPool.get()) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool.get();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vk::check(vkAllocateCommandBuffers(device.get(), &allocInfo, &buffer), "vkAllocateCommandBuffers");
}

CommandBuffer::~CommandBuffer() {
    if (buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device.get(), pool, 1, &buffer);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : device(other.device), pool(other.pool), buffer(other.buffer) {
    other.buffer = VK_NULL_HANDLE;
}

void CommandBuffer::beginRecording() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(buffer, &beginInfo) != VK_SUCCESS) {
        logger.fatal("Failed to begin recording command buffer!");
    }
}

void CommandBuffer::endRecording() {
    if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
        logger.fatal("Failed to finish recording command buffer!");
    }
}

} // namespace nacre::vk
