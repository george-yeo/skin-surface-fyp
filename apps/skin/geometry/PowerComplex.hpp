#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "skin/geometry/WeightedPoint.hpp"

namespace skin::geometry {

// Marks a missing neighbour: the infinite tet beyond a convex hull triangle.
inline constexpr uint32_t kInfinite = UINT32_MAX;

// ---- Delaunay side (the regular triangulation) ----

// Positions are double throughout; convert to float only for rendering.
struct DelaunayVertex {
    glm::dvec3 position;
    double weight;       // squared radius, same convention as WeightedPoint
    uint32_t inputIndex; // index into the points passed to build()
};

struct DelaunayEdge {
    std::array<uint32_t, 2> v;
};

struct DelaunayTriangle {
    std::array<uint32_t, 3> v;
};

struct DelaunayTet {
    std::array<uint32_t, 4> v;
};

// ---- Voronoi side (the power diagram) ----
//
// Duality is by index: each Voronoi element is the dual of the Delaunay
// element at the same index.
//   voronoiVertices()[i] <-> tets()[i]
//   voronoiEdges()[i]    <-> triangles()[i]
//   voronoiFaces()[i]    <-> edges()[i]
//   voronoiCells()[i]    <-> vertices()[i]
// So a face ring holds tet indices and a cell holds edge indices.

struct VoronoiEdge {
    // tets[0] is always finite. tets[1] == kInfinite means the edge is a ray
    // from voronoiVertices()[tets[0]] along rayDir (the dual triangle is on
    // the convex hull).
    std::array<uint32_t, 2> tets;
    glm::dvec3 rayDir;
};

struct VoronoiFace {
    // Tet indices in cyclic order around the dual edge. When unbounded (the
    // dual edge is on the convex hull) this is an open chain of the finite
    // tets, still in order; the two rays that close it off are the Voronoi
    // edges of the chain's two end triangles.
    std::vector<uint32_t> ring;
    bool bounded;
};

struct VoronoiCell {
    std::vector<uint32_t> faces; // edge indices == face indices
    bool bounded;
};

// The regular triangulation of a set of weighted points and its dual power
// diagram, exposed as plain indexed data. CGAL stays inside the .cpp.
class PowerComplex {
    public:
    PowerComplex();
    ~PowerComplex();

    PowerComplex(const PowerComplex&) = delete;
    PowerComplex& operator=(const PowerComplex&) = delete;

    // Clears and rebuilds from scratch, then logs sanity checks. The Voronoi
    // side and tets are only filled when the triangulation is 3D.
    void build(std::span<const WeightedPoint> points);

    int dimension() const;

    const std::vector<DelaunayVertex>& vertices() const;
    const std::vector<DelaunayEdge>& edges() const;
    const std::vector<DelaunayTriangle>& triangles() const;
    const std::vector<DelaunayTet>& tets() const;

    const std::vector<glm::dvec3>& voronoiVertices() const;
    const std::vector<VoronoiEdge>& voronoiEdges() const;
    const std::vector<VoronoiFace>& voronoiFaces() const;
    const std::vector<VoronoiCell>& voronoiCells() const;

    // Input indices that got no vertex: hidden by heavier neighbours, or
    // exact duplicates.
    const std::vector<uint32_t>& hiddenInputs() const;

    private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace skin::geometry
