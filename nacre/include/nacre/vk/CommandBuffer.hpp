#pragma once

#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class CommandBuffer {
    public:
    CommandBuffer(Device& device, CommandPool& commandPool);
    ~CommandBuffer();

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    CommandBuffer(CommandBuffer&&) noexcept;
    CommandBuffer& operator=(CommandBuffer&&) = delete;

    void beginRecording();
    void endRecording();
    VkCommandBuffer get() const { return buffer; }

    private:
    core::Logger logger{"CommandBuffer"};

    Device& device;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer buffer = VK_NULL_HANDLE;
};

} // namespace nacre::vk
