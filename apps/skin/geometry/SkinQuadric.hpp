#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

// The skin inside one mixed cell mu_X as the zero set of
//   F(x) = d^T A d + w_X,  d = x - z_X,  A = (I - P) / s - P / (1 - s),
// with P the projector onto the directions of aff X. F < 0 inside the skin
// body; F and grad F agree across shared walls, so the pieces form one C^1
// function on the box.
struct SkinQuadric {
    glm::dvec3 centre; // z_X
    double weight;     // w_X = |z_i - z_X|^2 - w_i for any i in X
    glm::dmat3 form;      // A
    glm::dmat3 projector; // P
    double formNorm;      // spectral norm of A

    double value(const glm::dvec3& x) const {
        const glm::dvec3 d = x - centre;
        return glm::dot(d, form * d) + weight;
    }
    glm::dvec3 gradient(const glm::dvec3& x) const { return 2.0 * (form * (x - centre)); }
};

// The quadric of the mixed cell of one Delaunay simplex (dimension 0..3,
// index into vertices()/edges()/triangles()/tets()).
SkinQuadric makeSkinQuadric(const PowerComplex& complex, uint32_t dimension, uint32_t simplex,
                            double s);

// One quadric per mixed cell, same order as mixed.cells.
std::vector<SkinQuadric> buildSkinQuadrics(const PowerComplex& complex, const MixedComplex& mixed,
                                           double s);

// Logs the worst disagreement of F and grad F between the two cells on every
// shared wall, and walls that are not on the box but have no neighbour.
void logWallAgreement(const MixedComplex& mixed, const MixedComplexTopology& topology,
                      const std::vector<SkinQuadric>& quadrics, const Aabb& box);

} // namespace skin::geometry
