#include <nacre/vk/Frame.hpp>
#include <nacre/vk/Check.hpp>

#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/CommandPool.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/vk/Swapchain.hpp>

namespace nacre::vk {

Frame::Frame(const RenderContext& renderContext)
    : renderContext(renderContext), device(renderContext.device) {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &imageAvailableSemaphore) !=
        VK_SUCCESS) {
        logger.fatal("Failed to create semaphore for a frame");
    }
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(device.get(), &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        logger.fatal("Failed to create fence for a frame");
    }

    commandBuffer = std::make_unique<CommandBuffer>(renderContext.commandPool.allocateBuffer());

    actionQueue = std::make_unique<core::ActionQueue>();
}

Frame::~Frame() {
    vkDestroySemaphore(device.get(), imageAvailableSemaphore, nullptr);
    vkDestroyFence(device.get(), inFlightFence, nullptr);
}

Frame::Frame(Frame&& other) noexcept
    : renderContext(other.renderContext),
      device(other.device),
      imageAvailableSemaphore(other.imageAvailableSemaphore),
      inFlightFence(other.inFlightFence),
      commandBuffer(std::move(other.commandBuffer)),
      actionQueue(std::move(other.actionQueue)) {
    other.imageAvailableSemaphore = VK_NULL_HANDLE;
    other.inFlightFence = VK_NULL_HANDLE;
}

void Frame::wait() {
    vk::check(vkWaitForFences(device.get(), 1, &inFlightFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");

    actionQueue->nextFrame();
    actionQueue->execute();
}

void Frame::reset() {
    vk::check(vkResetFences(device.get(), 1, &inFlightFence), "vkResetFences");
}

void Frame::beginRendering(VkImageView colorView, VkImageView depthView, bool clear) {
    VkExtent2D extent = renderContext.swapchain().getExtent();

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // later stages depth-test against it
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, extent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer->get(), &renderingInfo);
}

void Frame::updateViewport() {
    VkExtent2D extent = renderContext.swapchain().getExtent();
    VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
        0.0f, 1.0f};
    vkCmdSetViewport(commandBuffer->get(), 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(commandBuffer->get(), 0, 1, &scissor);
}

void Frame::endRendering() {
    vkCmdEndRendering(commandBuffer->get());
}

void Frame::defer(std::function<void()> action, uint32_t frames) {
    actionQueue->push(std::move(action), frames);
}

} // namespace nacre::vk
