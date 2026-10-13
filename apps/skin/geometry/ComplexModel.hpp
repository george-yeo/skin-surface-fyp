#pragma once

#include <string>
#include <vector>

#include "skin/geometry/BoundedVoronoi.hpp"
#include "skin/geometry/PowerComplex.hpp"
#include "skin/geometry/WeightedPoint.hpp"

namespace skin::geometry {

// The input points, their power complex, and the power diagram clipped to the
// skin box and to a larger box for drawing. Not movable: the visualiser holds
// references into it.
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

    void rebuild(double skinPadding, double displayPadding);
    void rebuildClipping(double skinPadding, double displayPadding);

    const std::vector<WeightedPoint>& points() const { return inputPoints; }
    const PowerComplex& complex() const { return powerComplex; }
    const BoundedVoronoi& skinVoronoi() const { return skinClip; }
    const BoundedVoronoi& displayVoronoi() const { return displayClip; }

    private:
    std::vector<WeightedPoint> inputPoints;
    PowerComplex powerComplex;
    BoundedVoronoi skinClip;
    BoundedVoronoi displayClip;
};

} // namespace skin::geometry
