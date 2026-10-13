#include "skin/vis/Spawn.hpp"

#include <nacre/assets/Handles.hpp>
#include <nacre/ecs/ECS.hpp>
#include <nacre/scene/ColorComponent.hpp>
#include <nacre/scene/LineComponent.hpp>
#include <nacre/scene/MeshComponent.hpp>
#include <nacre/scene/PointComponent.hpp>
#include <nacre/scene/TransformComponent.hpp>

namespace skin::vis {

nacre::ecs::Entity spawnPoint(nacre::ecs::ECS& ecs, const glm::vec3& position, float radius) {
    nacre::ecs::Entity entity = ecs.createEntity();

    auto transform = nacre::scene::TransformComponent{
        .position = position,
        .scale = {1.0f, 1.0f, 1.0f},
    };
    auto color = nacre::scene::ColorComponent{
        .color = {0.0f, 0.0f, 1.0f, 1.0f}, // blue
    };
    auto point = nacre::scene::PointComponent{
        .radius = radius,
    };

    ecs.addComponent(entity, transform);
    ecs.addComponent(entity, color);
    ecs.addComponent(entity, point);

    return entity;
}

nacre::ecs::Entity spawnMesh(nacre::ecs::ECS& ecs, const glm::vec3& position,
                             const nacre::assets::MeshHandle& handle,
                             const nacre::assets::MaterialHandle& materialHandle) {
    nacre::ecs::Entity entity = ecs.createEntity();

    auto transform = nacre::scene::TransformComponent{
        .position = position,
        .scale = {1.0f, 1.0f, 1.0f},
    };
    auto mesh = nacre::scene::MeshComponent{
        .handle = handle,
        .materialHandle = materialHandle,
    };

    ecs.addComponent(entity, transform);
    ecs.addComponent(entity, mesh);

    return entity;
}

nacre::ecs::Entity spawnLine(nacre::ecs::ECS& ecs, const glm::vec3& start, const glm::vec3& end,
                             float thickness) {
    nacre::ecs::Entity entity = ecs.createEntity();

    auto line = nacre::scene::LineComponent{
        .start = start,
        .end = end,
        .thickness = thickness,
    };

    ecs.addComponent(entity, line);

    return entity;
}

} // namespace skin::vis
