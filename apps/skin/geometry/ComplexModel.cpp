#include "skin/geometry/ComplexModel.hpp"

#include "skin/geometry/WeightedPointParser.hpp"

namespace skin::geometry {

void ComplexModel::load(const std::string& path) {
    inputPoints = WeightedPointParser::parseFile(path);
}

void ComplexModel::addPoint(const WeightedPoint& point) {
    inputPoints.push_back(point);
}

void ComplexModel::rebuild(double skinPadding, double displayPadding) {
    powerComplex.build(inputPoints);
    rebuildClipping(skinPadding, displayPadding);
}

void ComplexModel::rebuildClipping(double skinPadding, double displayPadding) {
    const Aabb skinBox = skinBounds(powerComplex, skinPadding);
    skinClip = buildBoundedVoronoi(powerComplex, skinBox);
    displayClip =
        buildBoundedVoronoi(powerComplex, displayBounds(powerComplex, skinBox, displayPadding));
}

} // namespace skin::geometry
