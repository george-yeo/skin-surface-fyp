#include "skin/geometry/SkinMesh.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <utility>

namespace skin::geometry {

namespace {

struct UnionFind {
    std::vector<uint32_t> parent;

    explicit UnionFind(size_t n) : parent(n) { std::iota(parent.begin(), parent.end(), 0u); }

    uint32_t find(uint32_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    void unite(uint32_t a, uint32_t b) { parent[find(a)] = find(b); }
};

} // namespace

SkinMeshStats computeSkinMeshStats(const SkinMesh& mesh) {
    SkinMeshStats stats;
    stats.triangles = mesh.triangles.size();

    std::map<std::pair<uint32_t, uint32_t>, uint32_t> edgeCounts;
    std::vector<bool> used(mesh.positions.size(), false);
    UnionFind components(mesh.positions.size());
    for (const auto& triangle : mesh.triangles) {
        for (size_t k = 0; k < 3; ++k) {
            const uint32_t a = triangle[k];
            const uint32_t b = triangle[(k + 1) % 3];
            used[a] = true;
            components.unite(a, b);
            ++edgeCounts[{std::min(a, b), std::max(a, b)}];
        }
    }
    stats.edges = edgeCounts.size();

    // V - E + F per component, keyed by the component's root vertex.
    std::map<uint32_t, int64_t> chi;
    for (uint32_t v = 0; v < used.size(); ++v) {
        if (used[v]) {
            ++stats.vertices;
            ++chi[components.find(v)];
        }
    }
    for (const auto& [edge, count] : edgeCounts) {
        --chi[components.find(edge.first)];
        if (count != 2) {
            ++stats.nonManifoldEdges;
        }
    }
    for (const auto& triangle : mesh.triangles) {
        ++chi[components.find(triangle[0])];
    }
    for (const auto& [root, value] : chi) {
        stats.eulerCharacteristics.push_back(value);
    }
    return stats;
}

} // namespace skin::geometry
