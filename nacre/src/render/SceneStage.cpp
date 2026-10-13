#include <nacre/render/SceneStage.hpp>

#include <algorithm>
#include <cstddef>

#include <nacre/assets/Material.hpp>
#include <nacre/assets/MaterialManager.hpp>
#include <nacre/assets/MeshManager.hpp>
#include <nacre/render/FrameResources.hpp>
#include <nacre/render/RenderScene.hpp>
#include <nacre/render/RenderSceneSync.hpp>
#include <nacre/vk/CommandBuffer.hpp>
#include <nacre/vk/DescriptorSet.hpp>
#include <nacre/vk/Frame.hpp>

namespace nacre::render {

void SceneStage::render(const FrameContext& context) {
    const bool opaque = pass == assets::MeshPass::Opaque;
    vk::Frame& frame = context.frame;
    FrameResources& resources = context.resources;
    const std::vector<DrawEntry::Mesh>& entries = context.renderScene.getMeshDrawEntries();

    frame.beginRendering(context.colorView, context.depthView, opaque);
    frame.updateViewport();
    if (opaque) {
        objectData.clear();
        for (const auto& entry : entries) {
            objectData.push_back({.model = entry.transform, .color = entry.color});
        }
        resources.uploadCamera(toCameraData(context.renderScene.getCamera()));
        resources.uploadObjects(objectData);
    }

    assets::MeshManager& meshes = context.renderScene.getMeshManager();
    assets::MaterialManager& materials = context.renderScene.getMaterialManager();
    vk::CommandBuffer& cmd = frame.getCommandBuffer();

    // An entry's object id is its index, i.e. the SSBO slot uploadObjects
    // wrote for it, so skipped entries can't shift the ids of later ones.
    const uint32_t count = static_cast<uint32_t>(
        std::min(entries.size(), static_cast<size_t>(resources.getMaxObjects())));
    for (uint32_t id = 0; id < count; ++id) {
        const DrawEntry::Mesh& entry = entries[id];
        const assets::MeshPass entryPass =
            entry.color.a < 1.0f ? assets::MeshPass::Transparent : assets::MeshPass::Opaque;
        if (entryPass != pass || !meshes.canRender(entry.meshHandle)) {
            continue;
        }

        assets::Material& material = materials.getMaterial(entry.materialHandle);
        const VkPipelineLayout layout = material.bind(cmd, context.frameIndex, pass);
        const VkDescriptorSet sceneSet = resources.getSceneSet().get();
        const VkDescriptorSet objectSet = resources.getObjectSet().get();
        vkCmdBindDescriptorSets(cmd.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1,
                                &sceneSet, 0, nullptr);
        vkCmdBindDescriptorSets(cmd.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1,
                                &objectSet, 0, nullptr);
        vkCmdPushConstants(cmd.get(), layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(uint32_t), &id);

        // Keeps the mesh alive until this frame's command buffer has finished.
        meshes.getMesh(entry.meshHandle).incrementReferenceCount();
        frame.defer([&meshes, handle = entry.meshHandle] {
            meshes.getMesh(handle).decrementReferenceCount();
        }, 1);

        meshes.bindMesh(entry.meshHandle, cmd);
        meshes.drawMesh(entry.meshHandle, cmd);
    }

    frame.endRendering();
}

} // namespace nacre::render
