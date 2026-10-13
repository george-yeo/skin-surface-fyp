#include <nacre/app/App.hpp>
#include <nacre/vk/Check.hpp>

#include <thread>
#include <vector>

// vulkan.h first: GLFW only declares glfwCreateWindowSurface if Vulkan is already included.
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <nacre/assets/MaterialManager.hpp>
#include <nacre/assets/MeshManager.hpp>
#include <nacre/ecs/ECS.hpp>
#include <nacre/platform/GlfwWindow.hpp>
#include <nacre/platform/InputManager.hpp>
#include <nacre/render/RasteriserRenderer.hpp>
#include <nacre/render/RenderScene.hpp>
#include <nacre/scene/ColorComponent.hpp>
#include <nacre/scene/LineComponent.hpp>
#include <nacre/scene/MeshComponent.hpp>
#include <nacre/scene/PointComponent.hpp>
#include <nacre/scene/TransformComponent.hpp>
#include <nacre/vk/Device.hpp>
#include <nacre/vk/RenderContext.hpp>
#include <nacre/vk/Surface.hpp>
#include <nacre/vk/Instance.hpp>

namespace nacre::app {

App::App(const AppConfig& config) {
    logger.print("Starting application");

    if (glfwInit() != GLFW_TRUE) {
        logger.fatal("Failed to initialise GLFW");
    }

    window = std::make_unique<platform::GlfwWindow>(config.width, config.height, config.title);

    const std::vector<const char*> extensions = platform::GlfwWindow::requiredInstanceExtensions();
    vulkanInstance = std::make_unique<vk::Instance>(extensions);

    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(vulkanInstance->get(), window->getNativeWindow(), nullptr,
                                &rawSurface) != VK_SUCCESS) {
        logger.fatal("Failed to create Vulkan surface");
    }
    surface = std::make_unique<vk::Surface>(*vulkanInstance, rawSurface);

    inputManager = std::make_unique<platform::InputManager>(*window);

    device = std::make_unique<vk::Device>(*vulkanInstance, *surface);

    auto rasteriserRenderer = std::make_unique<render::RasteriserRenderer>(
        *device, *window, *surface, [this] { onUi(); });
    render::RasteriserRenderer& rasteriserRendererRef = *rasteriserRenderer;
    renderer = std::move(rasteriserRenderer);

    meshManager = std::make_unique<assets::MeshManager>(*device);

    materialManager =
        std::make_unique<assets::MaterialManager>(rasteriserRendererRef.getContext());
    materialManager->loadDefaultMaterials();

    renderScene = std::make_unique<render::RenderScene>(
        rasteriserRendererRef.getContext().swapchain().getExtent(), *meshManager, *materialManager);

    ecs = std::make_unique<nacre::ecs::ECS>();
    ecs->registerComponent<scene::TransformComponent>();
    ecs->registerComponent<scene::MeshComponent>();
    ecs->registerComponent<scene::ColorComponent>();
    ecs->registerComponent<scene::PointComponent>();
    ecs->registerComponent<scene::LineComponent>();

    // ecs->registerSystem<RenderSystem>();
    // ecs->setSystemSignature<RenderSystem, TransformComponent>();
}

App::~App() {
    logger.print("Closing application");

    vkDeviceWaitIdle(device->get());

    renderScene.reset();

    meshManager.reset();
    materialManager.reset();

    renderer.reset();

    device.reset();
    surface.reset();

    inputManager.reset();
    window.reset();

    vulkanInstance.reset();

    glfwTerminate();

    logger.print("Application closed");
}

void App::run() {
    updateTimer.start();
    while (!window->shouldClose()) {
        update();
    }
    vk::check(vkDeviceWaitIdle(device->get()), "vkDeviceWaitIdle");
}

void App::update() {
    // Frame cap: skip until a full frame period has passed since the last
    // update. Yields rather than sleeps -- Windows' default ~15.6 ms sleep
    // granularity would overshoot an 8.3 ms frame badly.
    constexpr float targetFrameTime = 1.0f / 120.0f;
    float dt = static_cast<float>(updateTimer.getElapsedTime().count()) / 1000.0f;
    if (dt < targetFrameTime) {
        std::this_thread::yield();
        return;
    }

    updateTimer.restart();

    window->pollEvents();
    inputManager->update(dt);

    onUpdate(dt);

    ecs->update(dt);
    renderer->update(*ecs, *renderScene);
    meshManager->update();
}

platform::GlfwWindow& App::getWindow() const {
    return *window;
}

platform::InputManager& App::getInput() const {
    return *inputManager;
}

nacre::ecs::ECS& App::getEcs() const {
    return *ecs;
}

assets::MeshManager& App::getMeshManager() const {
    return *meshManager;
}

assets::MaterialManager& App::getMaterialManager() const {
    return *materialManager;
}

render::RenderScene& App::getRenderScene() const {
    return *renderScene;
}

} // namespace nacre::app
