#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "skin/geometry/BoundedVoronoi.hpp"
#include "skin/geometry/MixedComplex.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include "skin/geometry/SkinBuild.hpp"
#include "skin/geometry/SkinQuadric.hpp"
#include "skin/geometry/WeightedPoint.hpp"

namespace skin::geometry {

// The input points, their power complex, the power diagram clipped to the
// skin box and to a larger box for drawing, and the mixed complex in the skin
// box. Not movable: the visualiser holds references into it.
//
// Input that is not 3D gets ghost points: a far regular tetrahedron of
// strongly negative weight, so the complex is 3D. Their vertices have
// inputIndex >= points().size(); they add no skin and stay out of the skin box.
class ComplexModel {
    public:
    ComplexModel() = default;

    ComplexModel(const ComplexModel&) = delete;
    ComplexModel& operator=(const ComplexModel&) = delete;
    ComplexModel(ComplexModel&&) = delete;
    ComplexModel& operator=(ComplexModel&&) = delete;

    // Replaces the input points; throws if the file can't be parsed.
    void load(const std::string& path);
    void addPoint(const WeightedPoint& point);

    void rebuild(double skinPadding, double displayPadding, double s);
    void rebuildClipping(double skinPadding, double displayPadding, double s);
    void rebuildMixed(double s);

    // Meshing runs off the main thread on a copy of these; its results are
    // stored back with setSkin / setComparison.
    SkinInputs skinInputs() const;
    void setSkin(SkinBuild build) { skinBuild = std::move(build); }
    void clearSkin() { skinBuild = {}; }
    // Cleared when the mixed complex changes.
    void setComparison(SkinComparison result) { comparison = std::move(result); }

    const std::vector<WeightedPoint>& points() const { return inputPoints; }
    const PowerComplex& complex() const { return powerComplex; }
    const BoundedVoronoi& skinVoronoi() const { return skinClip; }
    const BoundedVoronoi& displayVoronoi() const { return displayClip; }
    const MixedComplex& mixedComplex() const { return mixed; }
    const MixedComplexTopology& mixedTopology() const { return topology; }
    const std::vector<SkinQuadric>& skinQuadrics() const { return quadrics; } // per mixed cell
    const SkinBuild& skin() const { return skinBuild; }
    const std::optional<SkinComparison>& skinComparison() const { return comparison; }

    uint32_t ghostCount() const { return ghosts; }
    bool isGhost(uint32_t vertex) const {
        return powerComplex.vertices()[vertex].inputIndex >= inputPoints.size();
    }

    private:
    std::vector<WeightedPoint> inputPoints;
    uint32_t ghosts = 0;
    PowerComplex powerComplex;
    BoundedVoronoi skinClip;
    BoundedVoronoi displayClip;
    MixedComplex mixed;
    MixedComplexTopology topology;
    std::vector<SkinQuadric> quadrics;
    SkinBuild skinBuild;
    std::optional<SkinComparison> comparison;
};

} // namespace skin::geometry
