#include <nacre/render/FrameResources.hpp>

#include <algorithm>

#include <nacre/shaders/CameraData.hpp>
#include <nacre/shaders/ObjectData.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/RenderContext.hpp>

namespace nacre::render {

FrameResources::FrameResources(const vk::RenderContext& renderContext, uint32_t objectCapacity)
    : device(renderContext.device), maxObjects(objectCapacity) {
    sceneUbo = std::make_unique<vk::Buffer>(
        device, sizeof(shaders::CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    objectSsbo = std::make_unique<vk::Buffer>(
        device, sizeof(shaders::ObjectData) * maxObjects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    sceneSet = std::make_unique<vk::DescriptorSet>(device, renderContext.descriptorPool,
                                                   renderContext.sceneSetLayout.get());
    sceneSet->writeBuffer(0, *sceneUbo); // update this if the layout changes

    objectSet = std::make_unique<vk::DescriptorSet>(device, renderContext.descriptorPool,
                                                    renderContext.objectSetLayout.get());
    objectSet->writeBuffer(0, *objectSsbo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // update this if the layout changes
}

FrameResources::~FrameResources() {
    sceneUbo.reset();
    sceneSet.reset();
    objectSsbo.reset();
    objectSet.reset();
}

void FrameResources::uploadCamera(const shaders::CameraData& camera) {
    sceneUbo->write(&camera, sizeof(shaders::CameraData));
}

void FrameResources::uploadObjects(std::span<const shaders::ObjectData> objects) {
    const size_t count = std::min(objects.size(), static_cast<size_t>(maxObjects));
    if (count > 0) {
        objectSsbo->write(objects.data(), count * sizeof(shaders::ObjectData));
    }
}

} // namespace nacre::render
