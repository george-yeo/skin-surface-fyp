#pragma once

#include <string>
#include <utility>
#include <vector>

#include <nacre/ecs/Types.hpp>
#include <nacre/ecs/fwd.hpp>
#include <nacre/assets/fwd.hpp>

namespace skin::vis {

// The entities (and MeshManager meshes) one visualisation layer created, so
// the layer can be torn down and rebuilt (or hidden) as a unit. Takes the
// managers per call rather than storing them, so it can be a plain member
// constructed before they exist.
struct EntityLayer {
    std::vector<nacre::ecs::Entity> entities;
    std::vector<std::string> meshes; // names registered with MeshManager

    void add(nacre::ecs::Entity entity) { entities.push_back(entity); }
    void addMesh(std::string name) { meshes.push_back(std::move(name)); }

    // Destroys every entity and releases every mesh this layer owns.
    void destroyAll(nacre::ecs::ECS& ecs, nacre::assets::MeshManager& meshManager);
};

} // namespace skin::vis
