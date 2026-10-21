#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "skin/vis/EntityLayer.hpp"
#include "skin/vis/UserSettings.hpp"
#include "skin/geometry/fwd.hpp"
#include <nacre/ecs/fwd.hpp>
#include <nacre/assets/fwd.hpp>

namespace skin::vis {

// Turns a PowerComplex, its clipped power diagrams and its input points into
// ECS entities, one EntityLayer per toggleable view. The skin clip feeds the
// mixed cells, the display clip the Voronoi layers. Holds references to
// everything it reads; the owner keeps them alive.
struct MixedCellCounts {
    std::array<uint32_t, 4> built{};   // by simplex dimension
    std::array<uint32_t, 4> clipped{}; // cut by the clip box
};

class ComplexVisualiser {
    public:
    ComplexVisualiser(nacre::ecs::ECS& ecs, nacre::assets::MeshManager& meshManager,
                      nacre::assets::MaterialManager& materialManager,
                      const geometry::ComplexModel& model);

    ComplexVisualiser(const ComplexVisualiser&) = delete;
    ComplexVisualiser& operator=(const ComplexVisualiser&) = delete;

    // Call after the complex has been rebuilt.
    void rebuildAll(const UserSettings& settings);

    // Rebuilds only the layers whose settings changed.
    void applySettings(const UserSettings& previous, const UserSettings& next);

    // Call when the model's skin has been replaced.
    void rebuildSkinLayer(const UserSettings& settings);

    // Zero while the mixed cell layer is hidden.
    const MixedCellCounts& mixedCellCounts() const { return mixedCounts; }

    private:
    nacre::ecs::ECS& ecs;
    nacre::assets::MeshManager& meshManager;
    nacre::assets::MaterialManager& materialManager;
    const geometry::ComplexModel& complexModel;
    const geometry::PowerComplex& complex;
    const geometry::BoundedVoronoi& skinVoronoi;
    const geometry::BoundedVoronoi& displayVoronoi;
    const std::vector<geometry::WeightedPoint>& points;

    EntityLayer pointLayer;
    EntityLayer weightLayer;
    EntityLayer delaunayFaceLayer;
    EntityLayer delaunayEdgeLayer;
    EntityLayer voronoiEdgeLayer;
    EntityLayer delaunayTetLayer;
    EntityLayer voronoiCellLayer;
    EntityLayer selectedCellLayer;
    EntityLayer mixedCellLayer;
    EntityLayer clipBoxLayer;
    EntityLayer skinLayer;

    MixedCellCounts mixedCounts;

    void rebuildPointLayer(const UserSettings& settings);
    void rebuildWeightLayer(const UserSettings& settings);
    void rebuildDelaunayFaceLayer(const UserSettings& settings);
    void rebuildDelaunayEdgeLayer(const UserSettings& settings);
    void rebuildVoronoiEdgeLayer(const UserSettings& settings);
    void rebuildDelaunayTetLayer(const UserSettings& settings);
    void rebuildVoronoiCellLayer(const UserSettings& settings);
    void rebuildSelectedCellLayer(const UserSettings& settings);
    void rebuildMixedCellLayer(const UserSettings& settings);
    void rebuildClipBoxLayer(const UserSettings& settings);

    template <size_t N>
    bool touchesGhost(const std::array<uint32_t, N>& simplex) const;

    void addLineEntity(EntityLayer& layer, const glm::vec3& start, const glm::vec3& end,
                       float thickness, const glm::vec4& color);
    void addMeshEntity(EntityLayer& layer, const std::string& name, const nacre::assets::MeshData& mesh,
                       const glm::vec4& color);
};

} // namespace skin::vis
