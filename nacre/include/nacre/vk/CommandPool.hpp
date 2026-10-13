#pragma once

#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class CommandPool {
    public:
    CommandPool(Device& device, uint32_t queueFamilyIndex);
    ~CommandPool();

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    CommandPool(CommandPool&&) = delete;
    CommandPool& operator=(CommandPool&&) = delete;

    CommandBuffer allocateBuffer();

    VkCommandPool get() const { return pool; }

    private:
    core::Logger logger{"CommandPool"};

    Device& device;
    VkCommandPool pool;
};

} // namespace nacre::vk
