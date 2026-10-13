#include <nacre/render/RenderScene.hpp>
#include <nacre/assets/MaterialManager.hpp>
#include <nacre/assets/MeshManager.hpp>

namespace nacre::render {

constexpr float size = 15.0f;

RenderScene::RenderScene(VkExtent2D initialExtent, assets::MeshManager& meshMgr,
                         assets::MaterialManager& matMgr)
    : meshManager(meshMgr), materialManager(matMgr) {
    createCamera(initialExtent);
}

RenderScene::~RenderScene() {}

void RenderScene::rebuild(const std::vector<DrawEntry::Mesh>& entries) {
    meshDrawEntries = entries;
}

void RenderScene::rebuildBalls(const std::vector<DrawEntry::Ball>& entries) {
    balls = entries;
}

void RenderScene::rebuildLines(const std::vector<DrawEntry::Line>& entries) {
    lines = entries;
}

void RenderScene::createCamera(VkExtent2D extent) {
    float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    logger.print("Creating camera with extent {}x{}, aspect ratio {}", extent.width, extent.height,
                 aspectRatio);
    // camera = std::make_unique<OrthographicCamera>(
    //     glm::vec3(0.0f, 2.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f),
    //     aspectRatio * size, size);
    camera = std::make_unique<scene::PerspectiveCamera>(glm::vec3(0.0f, 2.0f, 5.0f),
                                                 glm::vec3(0.0f, 0.0f, -1.0f),
                                                 glm::vec3(0.0f, 1.0f, 0.0f), 60.0f, aspectRatio);
}

void RenderScene::onSwapChainRecreated(VkExtent2D extent) {
    // Minimised windows can report a zero extent.
    if (extent.width > 0 && extent.height > 0) {
        camera->setAspectRatio(static_cast<float>(extent.width) /
                               static_cast<float>(extent.height));
    }
}

} // namespace nacre::render
