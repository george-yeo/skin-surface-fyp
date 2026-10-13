#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

#include <nacre/core/Logger.hpp>
#include <nacre/render/Renderer.hpp>
#include <nacre/ecs/fwd.hpp>
#include <nacre/vk/fwd.hpp>
#include <nacre/render/fwd.hpp>
#include <nacre/platform/fwd.hpp>

namespace nacre::render {

class RasteriserRenderer : public Renderer {
    public:
    RasteriserRenderer(vk::Device& device, platform::GlfwWindow& window,
                       const vk::Surface& surface, std::function<void()> drawUi);
    ~RasteriserRenderer() override;

    RasteriserRenderer(const RasteriserRenderer&) = delete;
    RasteriserRenderer& operator=(const RasteriserRenderer&) = delete;

    RasteriserRenderer(RasteriserRenderer&&) = delete;
    RasteriserRenderer& operator=(RasteriserRenderer&&) = delete;

    void update(const ecs::ECS& ecs, RenderScene& renderScene) override;

    const vk::RenderContext& getContext() const;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    private:
    core::Logger logger{"Rasteriser Renderer"};

    platform::GlfwWindow& window;
    const vk::Surface& surface;
    vk::Device& device;

    uint32_t currentFrameIndex = 0;

    // pImpl to keep the heavy Vulkan implementation details out of this header.
    struct Impl;
    std::unique_ptr<Impl> impl;

    void createShaderCache();
    void createSwapchain();
    void createFrameResources();
    void createFrames();
    void createCommandPool();
    void createDescriptorPool();
    void createContext();
    void createStages(std::function<void()> drawUi);
    void destroyFrames();

    // false while the surface is 0x0 (minimised): no swapchain until it has a size again.
    bool recreateSwapchain();
};

} // namespace nacre::render
