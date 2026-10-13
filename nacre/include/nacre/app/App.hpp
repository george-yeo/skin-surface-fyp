#pragma once

#include <memory>
#include <string>

#include <nacre/assets/fwd.hpp>
#include <nacre/core/Logger.hpp>
#include <nacre/core/Timer.hpp>
#include <nacre/ecs/fwd.hpp>
#include <nacre/platform/fwd.hpp>
#include <nacre/render/fwd.hpp>
#include <nacre/vk/fwd.hpp>

namespace nacre::app {

struct AppConfig {
    int width = 800;
    int height = 600;
    std::string title = "Nacre";
};

// Owns the window, device, renderer, managers, ECS and render scene, and runs
// the frame loop. Subclasses set themselves up in their own constructor.
class App {
    public:
    explicit App(const AppConfig& config);
    virtual ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void run();

    protected:
    virtual void onUpdate(double /*dt*/) {}
    // Called inside the ImGui frame.
    virtual void onUi() {}

    platform::GlfwWindow& getWindow() const;
    platform::InputManager& getInput() const;
    nacre::ecs::ECS& getEcs() const;
    assets::MeshManager& getMeshManager() const;
    assets::MaterialManager& getMaterialManager() const;
    render::RenderScene& getRenderScene() const;

    private:
    core::Logger logger{"App"};

    std::unique_ptr<vk::Instance> vulkanInstance;
    std::unique_ptr<platform::GlfwWindow> window;
    std::unique_ptr<vk::Surface> surface;
    std::unique_ptr<vk::Device> device;
    std::unique_ptr<assets::MeshManager> meshManager;
    std::unique_ptr<assets::MaterialManager> materialManager;

    std::unique_ptr<render::Renderer> renderer;
    std::unique_ptr<nacre::ecs::ECS> ecs;
    std::unique_ptr<platform::InputManager> inputManager;

    core::Timer updateTimer;

    std::unique_ptr<render::RenderScene> renderScene; // temp

    void update();
};

} // namespace nacre::app
