#pragma once

namespace nacre::assets {

// Opaque meshes draw first; transparent ones (object alpha < 1) after the
// ball/line impostors, blended, without writing depth.
enum class MeshPass { Opaque, Transparent };

} // namespace nacre::assets
