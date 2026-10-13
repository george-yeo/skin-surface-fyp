#include <nacre/vk/DescriptorPool.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/Pipeline.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/assets/Material.hpp>

namespace nacre::assets {

Material::Material(const vk::RenderContext& renderContext)
    : device{renderContext.device}, renderContext{renderContext} {}

Material::~Material() {
    pipeline.reset();
    descriptorSetLayout.reset();
}

const vk::Pipeline& Material::getPipeline() const { return *pipeline; }

void Material::allocateDescriptorSets() {
    auto layout = descriptorSetLayout->get();

    for (uint32_t i = 0; i < renderContext.framesInFlight; i++) {
        descriptorSets.emplace_back(device, renderContext.descriptorPool, layout);
    }
}

} // namespace nacre::assets
