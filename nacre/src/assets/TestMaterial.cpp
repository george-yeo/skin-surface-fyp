#include <memory>

#include <nacre/vk/Check.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/assets/Vertex.hpp>
#include <nacre/vk/Pipeline.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/vk/ShaderEffect.hpp>
#include <nacre/vk/Swapchain.hpp>
#include <nacre/assets/TestMaterial.hpp>

namespace nacre::assets {

struct MaterialUbo {
    glm::vec4 color; // std140 vec4, matching test_shader.frag
};

TestMaterial::TestMaterial(const vk::RenderContext& renderContext) : Material(renderContext) {
    shaderEffect = std::make_unique<vk::ShaderEffect>(renderContext.shaderCache, "test_shader.vert",
                                                      "test_shader.frag");

    for (uint32_t i = 0; i < renderContext.framesInFlight; i++) {
        ubos.emplace_back(
            device, sizeof(MaterialUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    createDescriptorSetLayout();

    allocateDescriptorSets();
    updateDescriptorSets();

    createPipeline(renderContext.swapchain().getFormat(),
                   renderContext.swapchain().getDepthFormat());
}

TestMaterial::~TestMaterial() {}

void TestMaterial::setWireframe(bool enabled) {
    // Only record the request here. This can be called mid-frame (the ImGui
    // checkbox is handled inside ImGuiPass, which runs after SceneStage has
    // already recorded vkCmdBindPipeline with the CURRENT pipeline into this
    // frame's command buffer). Destroying that pipeline right now would
    // invalidate a command buffer that's still being recorded, not just one
    // that's mid-execution on the GPU -- vkDeviceWaitIdle() only guards
    // against the latter. The actual rebuild happens in bind(), which is the
    // first thing to touch the pipeline each frame, so it's always safe there.
    pendingWireframe = enabled;
}

VkPipelineLayout TestMaterial::bind(vk::CommandBuffer& commandBuffer, uint32_t frameIndex,
                                    MeshPass pass) {
    if (pendingWireframe != wireframe) {
        wireframe = pendingWireframe;

        // Safe here: nothing in the CURRENT frame's command buffer has
        // referenced `pipeline` yet (bind() is called before anything else
        // touches it this frame), and vkDeviceWaitIdle() clears out any
        // previous frame's command buffer that might still be executing with
        // the old pipeline bound.
        vk::check(vkDeviceWaitIdle(device.get()), "vkDeviceWaitIdle");
        createPipeline(renderContext.swapchain().getFormat(),
                       renderContext.swapchain().getDepthFormat());
    }

    MaterialUbo ubo = {.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)};
    ubos[frameIndex].write(&ubo, sizeof(MaterialUbo));

    VkDescriptorSet descriptorSet = descriptorSets[frameIndex].get();

    const vk::Pipeline& active = pass == MeshPass::Transparent ? *transparentPipeline : *pipeline;
    vkCmdBindPipeline(commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, active.get());
    vkCmdBindDescriptorSets(commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            active.getLayout(), 1, 1, &descriptorSet, 0, nullptr);
    return active.getLayout();
}

void TestMaterial::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
    std::vector<VkDescriptorSetLayout> setLayouts{renderContext.sceneSetLayout.get(),
                                                  descriptorSetLayout->get(),
                                                  renderContext.objectSetLayout.get()};

    std::vector<VkVertexInputAttributeDescription> attributes; // todo

    attributes.emplace_back();
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    attributes.emplace_back();
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);

    attributes.emplace_back();
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, color);

    std::vector<VkVertexInputBindingDescription> bindings;

    bindings.emplace_back();
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(Vertex);
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = sizeof(uint32_t);

    vk::Pipeline::Config config{.colorFormat = colorFormat,
                            .depthFormat = depthFormat,
                            .polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
                            .vertexBindings = bindings,
                            .vertexAttributes = attributes,
                            .setLayouts = setLayouts,
                            .pushConstants = {range}};

    pipeline = std::make_unique<vk::Pipeline>(device.get(), *shaderEffect, config);

    // Meshes are wound counter-clockwise about their outward normals, and the
    // Y-flipped projection keeps that counter-clockwise on screen.
    vk::Pipeline::Config transparentConfig = config;
    transparentConfig.alphaBlend = true;
    transparentConfig.depthWrite = false;
    transparentConfig.cullMode = VK_CULL_MODE_BACK_BIT;
    transparentConfig.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    transparentPipeline =
        std::make_unique<vk::Pipeline>(device.get(), *shaderEffect, transparentConfig);
}

void TestMaterial::createDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings.push_back(uboBinding);

    descriptorSetLayout = std::make_unique<vk::DescriptorSetLayout>(device.get(), bindings);
}

void TestMaterial::updateDescriptorSets() {
    for (uint32_t i = 0; i < renderContext.framesInFlight; i++) {
        descriptorSets[i].writeBuffer(0, ubos[i]);
    }
}

} // namespace nacre::assets
