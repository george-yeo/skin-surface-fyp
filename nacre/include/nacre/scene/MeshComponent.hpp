#pragma once

#include <nacre/assets/Handles.hpp>

namespace nacre::scene {

struct MeshComponent {
    assets::MeshHandle handle;
    assets::MaterialHandle materialHandle;
};

} // namespace nacre::scene
