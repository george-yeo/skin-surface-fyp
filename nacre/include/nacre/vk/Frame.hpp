#pragma once

#include <nacre/core/Logger.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

#include <nacre/core/ActionQueue.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class Frame {
    public:
    explicit Frame(const RenderContext& renderContext);
    ~Frame();

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    Frame(Frame&&) noexcept;
    Frame& operator=(Frame&&) = delete;

    void wait();
    void reset();

    // clear = false loads the attachments left by earlier stages.
    void beginRendering(VkImageView colorView, VkImageView depthView, bool clear = true);
    void endRendering();
    void updateViewport();
    // Runs `action` after `frames` more waits on this frame's fence.
    void defer(std::function<void()> action, uint32_t frames);

    CommandBuffer& getCommandBuffer() { return *commandBuffer; }
    VkSemaphore getImageAvailableSemaphore() { return imageAvailableSemaphore; }
    VkFence getFence() { return inFlightFence; }

    private:
    core::Logger logger{"Frame"};

    const RenderContext& renderContext;
    Device& device;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    std::unique_ptr<CommandBuffer> commandBuffer;
    std::unique_ptr<core::ActionQueue> actionQueue;
};

} // namespace nacre::vk
