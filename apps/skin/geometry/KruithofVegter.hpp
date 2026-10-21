#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/SkinMesh.hpp"
#include "skin/geometry/SkinQuadric.hpp"
#include "skin/geometry/WeightedPoint.hpp"
#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

// CGAL's bounding octahedron for skin surfaces (Skin_surface_base_3::
// construct_bounding_box): six points of weight -1 at the centres' bounding
// box midpoint +- dr along each axis, dr = (dx + dy + dz + sqrt(max w) + 0.001)
// / s. Using the same points as CGAL makes the two triangulations comparable.
std::vector<WeightedPoint> boundingOctahedron(std::span<const WeightedPoint> points, double s);

// Kruithof-Vegter's tetrahedral complex [KV07, section 3.2] on the mixed cells
// of the input balls' simplices; the octahedron's cells are left out. Every
// vertex is the anchor point of a polyhedral cell mu_{tau, sigma}, identified
// by the pair (Delaunay anchor of tau, Voronoi anchor of sigma).
struct KvTetComplex {
    struct Cell {
        uint32_t dimension; // of the Delaunay simplex
        uint32_t simplex;   // in the complex built from points + octahedron
    };

    std::vector<glm::dvec3> points; // anchor points
    std::vector<std::array<uint32_t, 4>> tets;
    std::vector<uint32_t> tetCells; // index into cells
    std::vector<Cell> cells;
    std::vector<SkinQuadric> quadrics; // per cell

    size_t anchorPairs = 0; // distinct (Delaunay anchor, Voronoi anchor) pairs
    size_t weldedPairs = 0; // pairs that landed on another pair's point
    size_t skippedTets = 0; // flags whose anchors coincide
    // Condition 17 on every tet edge: the hypothesis of the isotopy theorem.
    // Slopes are exact zeros at anchors, so rounding is judged against
    // roundingScale = eps * M / l (M: largest centre coordinate used for an
    // anchor, l: extent of the input balls); only slopes beyond
    // CONDITION_FACTOR * roundingScale count as violations.
    size_t monotonicityViolations = 0;
    double worstMonotonicity = 0.0; // relative slope
    double roundingScale = 0.0;
    // Per cell |sum of tet volumes - vol(mu_X)| / vol(mu_X), worst over cells.
    double worstTilingError = 0.0;

    // Diagnostics, as indices into points followed by boundingOctahedron:
    // the simplices whose centres define each anchor point (the first pair
    // welded onto it), and each cell's simplex.
    struct AnchorLabel {
        std::vector<uint32_t> delaunay;
        std::vector<uint32_t> voronoi;
    };
    std::vector<AnchorLabel> pointLabels;
    std::vector<std::vector<uint32_t>> cellInputs;
};

// Logs anchor statistics, numerical checks of Lemmas 15-16 and Condition 17.
KvTetComplex buildKvTetComplex(std::span<const WeightedPoint> points, double s);

struct KvSkinResult {
    SkinMesh mesh; // triangleCell indexes the complex's cells
    size_t tets = 0;
    size_t monotonicityViolations = 0;
    size_t isotopyTestFailures = 0; // triangles failing [KV07, Lemma 23]
    bool cancelled = false;
};

// The Kruithof-Vegter mesh: marching tets on buildKvTetComplex, isotopic to
// the skin when Condition 17 holds on every edge [KV07, Thm 21], then
// `refinements` 1-to-9 rounds (refineKvMesh). Logs the Lemma 23 isotopy test.
// With a control, reports progress and stops early when asked.
KvSkinResult meshSkinKruithofVegter(std::span<const WeightedPoint> points, double s,
                                    int refinements = 0, MeshingControl* control = nullptr);

} // namespace skin::geometry
