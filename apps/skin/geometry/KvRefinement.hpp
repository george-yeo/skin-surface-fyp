#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "skin/geometry/fwd.hpp"

namespace skin::geometry {

struct KvRefinementResult {
    size_t unmovedPoints = 0;   // no transversal segment with a sign change through the point
    double worstResidual = 0.0; // max |F| / |grad F| over new vertices
    bool cancelled = false;
};

// Refines a coarse Kruithof-Vegter mesh by `rounds` 1-to-9 splits, i.e.
// Kobbelt's sqrt(3) step applied twice [Kob00], as in [KV07, section 3.4,
// Fig. 7]. Each triangle lies in one tet of the complex (triangleTets, kept in
// step with the mesh) and so do its nine children. Every new vertex moves along
// the transversal segment of that tet through it onto the skin, at the
// closed-form root of F on the segment [KV07, Lemma 24]; old vertices stay.
// values holds F at the complex's points, as the marcher left them.
KvRefinementResult refineKvMesh(const KvTetComplex& complex, std::span<const double> values,
                                int rounds, SkinMesh& mesh, std::vector<uint32_t>& triangleTets,
                                MeshingControl* control = nullptr);

// Triangles failing the isotopy test of [KV07, Lemma 23] against their own tet:
// the triangle's plane must have the tet's inside vertices strictly on its
// inner side and the outside ones strictly on its outer side. Sufficient for
// every transversal segment of the tet to meet the mesh once, not necessary.
size_t countIsotopyTestFailures(const KvTetComplex& complex, std::span<const double> values,
                                const SkinMesh& mesh, std::span<const uint32_t> triangleTets);

} // namespace skin::geometry
