#include <nacre/render/RenderSceneSync.hpp>

#include <nacre/scene/ColorComponent.hpp>
#include <nacre/scene/LineComponent.hpp>
#include <nacre/scene/MeshComponent.hpp>
#include <nacre/scene/PointComponent.hpp>
#include <nacre/scene/TransformComponent.hpp>
#include <nacre/ecs/ECS.hpp>
#include <nacre/render/RenderScene.hpp>
#include <nacre/scene/Camera.hpp>

namespace nacre::render {

void syncRenderScene(const ecs::ECS& ecs, RenderScene& renderScene) {
    std::vector<DrawEntry::Mesh> drawEntries;

    for (ecs::Entity entity : ecs.view<scene::TransformComponent, scene::MeshComponent>()) {
        const auto& transform = ecs.getComponent<scene::TransformComponent>(entity);
        const auto& mesh = ecs.getComponent<scene::MeshComponent>(entity);
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        if (ecs.hasComponent<scene::ColorComponent>(entity)) {
            color = ecs.getComponent<scene::ColorComponent>(entity).color;
        }
        drawEntries.push_back(DrawEntry::Mesh{.meshHandle = mesh.handle,
                                              .materialHandle = mesh.materialHandle,
                                              .transform = transform.toMatrix(),
                                              .color = color});
    }

    renderScene.rebuild(drawEntries);

    std::vector<DrawEntry::Ball> balls;

    for (ecs::Entity entity : ecs.view<scene::TransformComponent, scene::PointComponent>()) {
        const auto& transform = ecs.getComponent<scene::TransformComponent>(entity);
        const auto& point = ecs.getComponent<scene::PointComponent>(entity);
        balls.push_back(DrawEntry::Ball{.position = transform.position,
                                        .radius = std::max(point.radius, 0.0f)});
    }

    renderScene.rebuildBalls(balls);

    std::vector<DrawEntry::Line> lines;

    for (ecs::Entity entity : ecs.view<scene::LineComponent>()) {
        const auto& line = ecs.getComponent<scene::LineComponent>(entity);
        // Optional per-line colour, same pattern as meshes above.
        glm::vec4 color{0.85f, 0.85f, 0.85f, 1.0f};
        if (ecs.hasComponent<scene::ColorComponent>(entity)) {
            color = ecs.getComponent<scene::ColorComponent>(entity).color;
        }
        lines.push_back(DrawEntry::Line{.start = line.start,
                                        .end = line.end,
                                        .thickness = std::max(line.thickness, 0.0f),
                                        .color = color});
    }

    renderScene.rebuildLines(lines);
}

shaders::CameraData toCameraData(const scene::Camera& camera) {
    return shaders::CameraData{
        .view = camera.view(),
        .projection = camera.projection(),
        .position = glm::vec4(camera.getOrigin(), 1.0f),
    };
}

} // namespace nacre::render
