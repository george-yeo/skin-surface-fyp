#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <nacre/ecs/fwd.hpp>
#include <nacre/vk/fwd.hpp>
#include <nacre/render/fwd.hpp>

namespace nacre::render {

struct FrameContext {
    VkCommandBuffer cmd;
    VkImageView colorView;
    VkImageView depthView;
    VkExtent2D extent;
    uint32_t frameIndex;
    vk::Frame& frame;
    FrameResources& resources;
    const ecs::ECS& ecs;
    RenderScene& renderScene;
};

class FrameStage {
    public:
    virtual ~FrameStage() = default;

    virtual void render(const FrameContext& context) = 0;
};

} // namespace nacre::render
