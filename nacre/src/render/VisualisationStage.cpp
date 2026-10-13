#include <nacre/render/VisualisationStage.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

#include <nacre/vk/Buffer.hpp>
#include <nacre/vk/DescriptorSet.hpp>
#include <nacre/vk/DescriptorSetLayout.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/render/DrawEntry.hpp>
#include <nacre/render/RenderScene.hpp>
#include <nacre/render/RenderSceneSync.hpp>
#include <nacre/shaders/CameraData.hpp>
#include <nacre/vk/Pipeline.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/vk/ShaderCache.hpp>
#include <nacre/vk/ShaderEffect.hpp>
#include <nacre/vk/Swapchain.hpp>

namespace nacre::render {

namespace {

constexpr size_t MAX_BALLS = 1000;
constexpr size_t MAX_LINES = 16384; // Delaunay + Voronoi edges; 48 B each, ~768 KB per frame
constexpr uint32_t QUAD_VERTEX_COUNT = 6; // two triangles, no vertex/index buffer
constexpr uint32_t BOX_VERTEX_COUNT = 36; // twelve triangles, no vertex/index buffer

struct BallPushConstants {
    float minRadius;
    float maxRadius;
};

} // namespace

VisualisationStage::VisualisationStage(const vk::RenderContext& renderContext)
    : renderContext(renderContext), device(renderContext.device) {
    ballShaderEffect = std::make_unique<vk::ShaderEffect>(
        renderContext.shaderCache, "visualisation_ball.vert", "visualisation_ball.frag");
    lineShaderEffect = std::make_unique<vk::ShaderEffect>(
        renderContext.shaderCache, "visualisation_line.vert", "visualisation_line.frag");

    createDescriptorSetLayout();
    createFrameResources();
    createPipelines();
}

VisualisationStage::~VisualisationStage() = default;

void VisualisationStage::createDescriptorSetLayout() {
    // Both bindings need to be visible to VERTEX (billboard sizing/orienting)
    // and FRAGMENT (the actual ray intersection) -- unlike the old
    // fullscreen-triangle version, where only the fragment stage touched
    // these buffers.
    std::vector<VkDescriptorSetLayoutBinding> bindings{
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    primitiveSetLayout = std::make_unique<vk::DescriptorSetLayout>(device.get(), bindings);
}

void VisualisationStage::createFrameResources() {
    for (uint32_t i = 0; i < renderContext.framesInFlight; i++) {
        sceneUbos.push_back(std::make_unique<vk::Buffer>(
            device, sizeof(shaders::CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        sceneSets.push_back(
            std::make_unique<vk::DescriptorSet>(device, renderContext.descriptorPool,
                                                renderContext.sceneSetLayout.get()));
        sceneSets.back()->writeBuffer(0, *sceneUbos.back());

        ballSsbos.push_back(std::make_unique<vk::Buffer>(
            device, sizeof(DrawEntry::Ball) * MAX_BALLS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        lineSsbos.push_back(std::make_unique<vk::Buffer>(
            device, sizeof(DrawEntry::Line) * MAX_LINES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        primitiveSets.push_back(std::make_unique<vk::DescriptorSet>(
            device, renderContext.descriptorPool, primitiveSetLayout->get()));
        primitiveSets.back()->writeBuffer(0, *ballSsbos.back(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        primitiveSets.back()->writeBuffer(1, *lineSsbos.back(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
}

void VisualisationStage::createPipelines() {
    VkPushConstantRange ballRange{};
    ballRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    ballRange.offset = 0;
    ballRange.size = sizeof(BallPushConstants);

    vk::Pipeline::Config ballConfig{
        .colorFormat = renderContext.swapchain().getFormat(),
        .depthFormat = renderContext.swapchain().getDepthFormat(),
        .polygonMode = VK_POLYGON_MODE_FILL,
        .setLayouts = {renderContext.sceneSetLayout.get(), primitiveSetLayout->get()},
        .pushConstants = {ballRange},
    };
    ballPipeline = std::make_unique<vk::Pipeline>(device.get(), *ballShaderEffect, ballConfig);

    // No push constants: lines don't have a color gradient (flat shade).
    vk::Pipeline::Config lineConfig{
        .colorFormat = renderContext.swapchain().getFormat(),
        .depthFormat = renderContext.swapchain().getDepthFormat(),
        .polygonMode = VK_POLYGON_MODE_FILL,
        .setLayouts = {renderContext.sceneSetLayout.get(), primitiveSetLayout->get()},
    };
    linePipeline = std::make_unique<vk::Pipeline>(device.get(), *lineShaderEffect, lineConfig);
}

void VisualisationStage::render(const FrameContext& context) {
    uint32_t frameIndex = context.frameIndex;

    const shaders::CameraData cameraData = toCameraData(context.renderScene.getCamera());
    sceneUbos[frameIndex]->write(&cameraData, sizeof(shaders::CameraData));

    const auto& balls = context.renderScene.getBalls();
    size_t ballCount = std::min(balls.size(), MAX_BALLS);
    if (balls.size() > MAX_BALLS && !warnedBallOverflow) {
        logger.warn("{} balls exceeds MAX_BALLS ({}); the rest are not drawn", balls.size(),
                    MAX_BALLS);
        warnedBallOverflow = true;
    }
    if (ballCount > 0) {
        ballSsbos[frameIndex]->write(balls.data(), ballCount * sizeof(DrawEntry::Ball));
    }

    const auto& lines = context.renderScene.getLines();
    size_t lineCount = std::min(lines.size(), MAX_LINES);
    if (lines.size() > MAX_LINES && !warnedLineOverflow) {
        logger.warn("{} lines exceeds MAX_LINES ({}); the rest are not drawn", lines.size(),
                    MAX_LINES);
        warnedLineOverflow = true;
    }
    if (lineCount > 0) {
        lineSsbos[frameIndex]->write(lines.data(), lineCount * sizeof(DrawEntry::Line));
    }

    float minRadius = std::numeric_limits<float>::max();
    float maxRadius = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < ballCount; i++) {
        minRadius = std::min(minRadius, balls[i].radius);
        maxRadius = std::max(maxRadius, balls[i].radius);
    }
    if (ballCount == 0) {
        minRadius = 0.0f;
        maxRadius = 1.0f;
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = context.colorView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = context.depthView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, context.extent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(context.cmd, &renderingInfo);

    VkViewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(context.extent.width),
                        static_cast<float>(context.extent.height),
                        0.0f,
                        1.0f};
    vkCmdSetViewport(context.cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, context.extent};
    vkCmdSetScissor(context.cmd, 0, 1, &scissor);

    VkDescriptorSet sets[] = {sceneSets[frameIndex]->get(), primitiveSets[frameIndex]->get()};

    vkCmdBindPipeline(context.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ballPipeline->get());
    vkCmdBindDescriptorSets(context.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ballPipeline->getLayout(), 0, 2, sets, 0, nullptr);
    BallPushConstants ballPc{.minRadius = minRadius, .maxRadius = maxRadius};
    vkCmdPushConstants(context.cmd, ballPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(BallPushConstants), &ballPc);
    vkCmdDraw(context.cmd, QUAD_VERTEX_COUNT, static_cast<uint32_t>(ballCount), 0, 0);

    vkCmdBindPipeline(context.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline->get());
    vkCmdBindDescriptorSets(context.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            linePipeline->getLayout(), 0, 2, sets, 0, nullptr);
    vkCmdDraw(context.cmd, BOX_VERTEX_COUNT, static_cast<uint32_t>(lineCount), 0, 0);

    vkCmdEndRendering(context.cmd);
}

} // namespace nacre::render
