#pragma once

#include <cstdint>

namespace nacre::assets {

struct MeshHandle { // todo: cloning and reference counting
    uint32_t id;
};

struct MaterialHandle { // todo: cloning and reference counting
    uint32_t id;
};

} // namespace nacre::assets
