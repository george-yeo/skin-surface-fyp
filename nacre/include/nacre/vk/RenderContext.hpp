#pragma once

#include <cstdint>
#include <memory>

#include <nacre/vk/Swapchain.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::vk {

class RenderContext {
    public:
    RenderContext(Device& device, CommandPool& commandPool, DescriptorPool& descriptorPool,
                  DescriptorSetLayout& sceneSetLayout, DescriptorSetLayout& objectSetLayout,
                  ShaderCache& shaderCache, uint32_t framesInFlight,
                  const std::unique_ptr<Swapchain>& swapchainSlot)
        : device(device), commandPool(commandPool), descriptorPool(descriptorPool),
          sceneSetLayout(sceneSetLayout), objectSetLayout(objectSetLayout),
          shaderCache(shaderCache), framesInFlight(framesInFlight), swapchainSlot(&swapchainSlot) {}

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    Device& device;
    CommandPool& commandPool;
    DescriptorPool& descriptorPool;
    DescriptorSetLayout& sceneSetLayout;
    DescriptorSetLayout& objectSetLayout;
    ShaderCache& shaderCache;
    const uint32_t framesInFlight;

    // Read through the owner's slot: recreating the swapchain replaces the object.
    const Swapchain& swapchain() const { return **swapchainSlot; }

    private:
    const std::unique_ptr<Swapchain>* swapchainSlot;
};

} // namespace nacre::vk
