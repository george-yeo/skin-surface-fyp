#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

#include <nacre/vk/Buffer.hpp>
#include <nacre/scene/Camera.hpp>
#include <nacre/render/DrawEntry.hpp>
#include <nacre/assets/fwd.hpp>
#include <nacre/render/fwd.hpp>

namespace nacre::render {

class RenderScene {
    public:
    RenderScene(VkExtent2D initialExtent, assets::MeshManager& meshMgr,
                assets::MaterialManager& matMgr);
    ~RenderScene();

    void rebuild(const std::vector<DrawEntry::Mesh>& entries); // temp: make adapter
    void rebuildBalls(const std::vector<DrawEntry::Ball>& entries);
    void rebuildLines(const std::vector<DrawEntry::Line>& entries);

    void onSwapChainRecreated(VkExtent2D extent);

    assets::MeshManager& getMeshManager() const { return meshManager; }
    assets::MaterialManager& getMaterialManager() const { return materialManager; }

    const std::vector<DrawEntry::Mesh>& getMeshDrawEntries() const { return meshDrawEntries; }
    const std::vector<DrawEntry::Ball>& getBalls() const { return balls; }
    const std::vector<DrawEntry::Line>& getLines() const { return lines; }

    scene::Camera& getCamera() { return *camera; }
    const scene::Camera& getCamera() const { return *camera; }

    private:
    core::Logger logger{"RenderScene"};

    std::unique_ptr<scene::Camera> camera;
    assets::MeshManager& meshManager;
    assets::MaterialManager& materialManager;
    std::vector<DrawEntry::Mesh> meshDrawEntries;
    std::vector<DrawEntry::Ball> balls;
    std::vector<DrawEntry::Line> lines;

    void createCamera(VkExtent2D extent);
};

} // namespace nacre::render
