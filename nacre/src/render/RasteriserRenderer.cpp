#include <nacre/render/RasteriserRenderer.hpp>
#include <nacre/vk/Check.hpp>

#include <utility>

#include <nacre/platform/GlfwWindow.hpp>
#include <nacre/core/Logger.hpp>
#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/CommandPool.hpp>
#include <nacre/vk/DepthImage.hpp>
#include <nacre/vk/DescriptorPool.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/vk/Frame.hpp>
#include <nacre/render/FrameResources.hpp>
#include <nacre/render/FrameStage.hpp>
#include <nacre/vk/Image.hpp>
#include <nacre/render/RenderScene.hpp>
#include <nacre/render/RenderSceneSync.hpp>
#include <nacre/render/SceneStage.hpp>
#include <nacre/vk/ShaderEffect.hpp>
#include <nacre/vk/Surface.hpp>
#include <nacre/vk/Swapchain.hpp>
#include <nacre/render/ImGuiPass.hpp>
#include <nacre/render/VisualisationStage.hpp>
#include <nacre/vk/Instance.hpp>
#include <nacre/ecs/Types.hpp>

namespace nacre::render {

namespace {

// Every mesh draw entry comes from its own ECS entity, so sizing the object
// SSBO to MAX_ENTITIES means it can't overflow (80 B each, ~5 MB per frame).
constexpr uint32_t MAX_OBJECTS = static_cast<uint32_t>(ecs::MAX_ENTITIES);

VkExtent2D currentSurfaceExtent(const vk::Device& device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities{};
    vk::check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.getPhysicalDevice(), surface, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    return capabilities.currentExtent;
}

void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
                     VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                     VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {aspect, 0, 1, 0, 1};

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

} // namespace

struct RasteriserRenderer::Impl {
    std::unique_ptr<vk::Swapchain> swapchain;
    std::unique_ptr<vk::CommandPool> commandPool;
    std::unique_ptr<vk::DescriptorPool> descriptorPool;
    std::vector<std::unique_ptr<FrameStage>> stages;

    std::vector<vk::Frame> frames;
    std::vector<std::unique_ptr<FrameResources>> frameResources; // one per frame in flight

    std::unique_ptr<vk::ShaderCache> shaderCache;

    std::unique_ptr<vk::DescriptorSetLayout> sceneSetLayout;
    std::unique_ptr<vk::DescriptorSetLayout> objectSetLayout;

    bool swapchainSuspended = false;

    std::unique_ptr<vk::RenderContext> context;
};

RasteriserRenderer::RasteriserRenderer(vk::Device& device, platform::GlfwWindow& window,
                                       const vk::Surface& surface, std::function<void()> drawUi)
    : window(window), surface(surface), device(device), impl(std::make_unique<Impl>()) {
    impl->sceneSetLayout = std::make_unique<vk::DescriptorSetLayout>(
        device.get(),
        std::vector<VkDescriptorSetLayoutBinding>{
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        });

    impl->objectSetLayout = std::make_unique<vk::DescriptorSetLayout>(
        device.get(),
        std::vector<VkDescriptorSetLayoutBinding>{
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        });

    createSwapchain();
    createCommandPool();
    createDescriptorPool();
    createShaderCache();
    createContext();
    createFrameResources();
    createFrames();
    createStages(std::move(drawUi));
}

RasteriserRenderer::~RasteriserRenderer() {
    impl->stages.clear();

    destroyFrames();
    impl->frameResources.clear();

    impl->context.reset();
    impl->shaderCache.reset();

    impl->descriptorPool.reset();
    impl->commandPool.reset();

    logger.print("Destroying swapchain");
    impl->swapchain.reset();

    impl->objectSetLayout.reset();
    impl->sceneSetLayout.reset();

    logger.print("Rasteriser Renderer destroyed");
}

void RasteriserRenderer::createShaderCache() {
    impl->shaderCache = std::make_unique<vk::ShaderCache>(device);
}

void RasteriserRenderer::createContext() {
    impl->context = std::make_unique<vk::RenderContext>(
        device, *impl->commandPool, *impl->descriptorPool, *impl->sceneSetLayout,
        *impl->objectSetLayout, *impl->shaderCache, MAX_FRAMES_IN_FLIGHT, impl->swapchain);
}

void RasteriserRenderer::createSwapchain() {
    impl->swapchain =
        std::make_unique<vk::Swapchain>(device, surface.get());
}

void RasteriserRenderer::createFrameResources() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        impl->frameResources.push_back(std::make_unique<FrameResources>(*impl->context, MAX_OBJECTS));
    }
}

void RasteriserRenderer::createFrames() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        impl->frames.emplace_back(*impl->context);
    }

    logger.print("Created {} frames", MAX_FRAMES_IN_FLIGHT);
}

void RasteriserRenderer::createCommandPool() {
    impl->commandPool = std::make_unique<vk::CommandPool>(device, device.getGraphicsFamilyIndex());
}

void RasteriserRenderer::createDescriptorPool() {
    impl->descriptorPool =
        std::make_unique<vk::DescriptorPool>(device, 1000,
                                         std::vector<VkDescriptorPoolSize>{
                                             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         });
}

void RasteriserRenderer::createStages(std::function<void()> drawUi) {
    impl->stages.push_back(std::make_unique<SceneStage>(assets::MeshPass::Opaque));
    impl->stages.push_back(std::make_unique<VisualisationStage>(*impl->context));
    impl->stages.push_back(std::make_unique<SceneStage>(assets::MeshPass::Transparent));
    impl->stages.push_back(std::make_unique<ImGuiPass>(window, *impl->context, std::move(drawUi)));
}

const vk::RenderContext& RasteriserRenderer::getContext() const {
    return *impl->context;
}

void RasteriserRenderer::destroyFrames() {
    impl->frames.clear();
    logger.print("Frames destroyed");
}

bool RasteriserRenderer::recreateSwapchain() {
    const VkExtent2D extent = currentSurfaceExtent(device, surface.get());
    if (extent.width == 0 || extent.height == 0) {
        impl->swapchainSuspended = true;
        return false;
    }

    logger.print("Recreating swapchain");
    vk::check(vkDeviceWaitIdle(device.get()), "vkDeviceWaitIdle");

    // Frames hold nothing swapchain-dependent, so they (and their pending
    // deferred actions) survive the recreation.
    impl->swapchain = std::make_unique<vk::Swapchain>(
        device, surface.get(), impl->swapchain->get());
    impl->swapchainSuspended = false;
    return true;
}

void RasteriserRenderer::update(const ecs::ECS& ecs, RenderScene& renderScene) {
    if (impl->swapchainSuspended) {
        if (!recreateSwapchain()) {
            return;
        }
        renderScene.onSwapChainRecreated(impl->swapchain->getExtent());
    }

    auto& currentFrame = impl->frames[currentFrameIndex];
    currentFrame.wait();

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device.get(), impl->swapchain->get(), UINT64_MAX,
                                            currentFrame.getImageAvailableSemaphore(),
                                            VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (recreateSwapchain()) {
            renderScene.onSwapChainRecreated(impl->swapchain->getExtent());
        }
        return;
    }
    // SUBOPTIMAL still delivers a usable image; the swapchain is recreated after present.
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        logger.fatal("Failed to acquire swapchain image! VkResult = {}", static_cast<int>(result));
    }

    currentFrame.reset();

    syncRenderScene(ecs, renderScene);

    auto& commandBuffer = currentFrame.getCommandBuffer();
    VkCommandBuffer cmd = commandBuffer.get();

    vk::Image& colorImage = impl->swapchain->getImages()[imageIndex];
    vk::DepthImage& depthImage = impl->swapchain->getDepthImages()[imageIndex];

    commandBuffer.beginRecording();

    // srcStage matches the acquire semaphore's wait stage, so the layout
    // transition can't start before the presentation engine releases the image.
    transitionImage(cmd, colorImage.get(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    // Ordered after the depth writes of the previous submission that used this image.
    transitionImage(
        cmd, depthImage.get(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    FrameContext context{
        .cmd = cmd,
        .colorView = colorImage.getView(),
        .depthView = depthImage.getView(),
        .extent = impl->swapchain->getExtent(),
        .frameIndex = currentFrameIndex,
        .frame = currentFrame,
        .resources = *impl->frameResources[currentFrameIndex],
        .ecs = ecs,
        .renderScene = renderScene,
    };

    for (size_t i = 0; i < impl->stages.size(); i++) {
        if (i > 0) {
            // Two dynamic-rendering scopes on the same attachment have no implicit
            // ordering the way VkRenderPass subpass dependencies would provide, so
            // insert an explicit same-layout barrier between every pair of stages,
            // for every attachment more than one stage touches. Color is written
            // by every stage; depth is now also read+written by VisualisationStage
            // (not just SceneStage), so it needs the same treatment.
            transitionImage(
                cmd, colorImage.get(), VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);

            transitionImage(cmd, depthImage.get(), VK_IMAGE_ASPECT_DEPTH_BIT,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
        }

        impl->stages[i]->render(context);
    }

    transitionImage(
        cmd, colorImage.get(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);

    commandBuffer.endRecording();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {currentFrame.getImageAvailableSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {impl->swapchain->getRenderFinishedSemaphore(imageIndex)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult res =
        vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, currentFrame.getFence());
    if (res != VK_SUCCESS) {
        logger.fatal("Failed to submit draw command buffer! VkResult = {}", static_cast<int>(res));
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {impl->swapchain->get()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (recreateSwapchain()) {
            renderScene.onSwapChainRecreated(impl->swapchain->getExtent());
        }
    } else if (result != VK_SUCCESS) {
        logger.fatal("Failed to present! VkResult = {}", static_cast<int>(result));
    }

    currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace nacre::render
