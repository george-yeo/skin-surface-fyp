#include <nacre/vk/CommandPool.hpp>
#include <nacre/vk/Check.hpp>

#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/Device.hpp>

namespace nacre::vk {

CommandPool::CommandPool(Device& device, uint32_t queueFamilyIndex) : device(device) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // re-record each frame

    vk::check(vkCreateCommandPool(device.get(), &poolInfo, nullptr, &pool), "vkCreateCommandPool");

    logger.print("CommandPool created");
}

CommandPool::~CommandPool() {
    vkDestroyCommandPool(device.get(), pool, nullptr);
    logger.print("CommandPool destroyed");
}

CommandBuffer CommandPool::allocateBuffer() {
    return CommandBuffer{device, *this};
}

} // namespace nacre::vk
