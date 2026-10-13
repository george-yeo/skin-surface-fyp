#pragma once

#include <glm/glm.hpp>

#include <nacre/assets/fwd.hpp>
#include <nacre/ecs/Types.hpp>
#include <nacre/ecs/fwd.hpp>

namespace skin::vis {

nacre::ecs::Entity spawnPoint(nacre::ecs::ECS& ecs, const glm::vec3& position, float radius);
nacre::ecs::Entity spawnMesh(nacre::ecs::ECS& ecs, const glm::vec3& position,
                             const nacre::assets::MeshHandle& handle,
                             const nacre::assets::MaterialHandle& materialHandle);
nacre::ecs::Entity spawnLine(nacre::ecs::ECS& ecs, const glm::vec3& start, const glm::vec3& end,
                             float thickness);

} // namespace skin::vis
