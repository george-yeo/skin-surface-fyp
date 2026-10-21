#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace skin::geometry {

// Merges points closer than eps (max-norm) into shared ids. A hash grid of
// side 2 eps: a match within eps lies in one of the 27 neighbouring cells.
class PointWelder {
    public:
    explicit PointWelder(double eps) : eps(eps), cellSize(2.0 * eps) {}

    uint32_t idOf(const glm::dvec3& p) {
        const GridKey key = keyOf(p);
        for (int64_t dx = -1; dx <= 1; ++dx) {
            for (int64_t dy = -1; dy <= 1; ++dy) {
                for (int64_t dz = -1; dz <= 1; ++dz) {
                    const auto it = grid.find({key.x + dx, key.y + dy, key.z + dz});
                    if (it == grid.end()) {
                        continue;
                    }
                    for (uint32_t id : it->second) {
                        const glm::dvec3 d = glm::abs(points[id] - p);
                        if (std::max({d.x, d.y, d.z}) <= eps) {
                            return id;
                        }
                    }
                }
            }
        }
        const auto id = static_cast<uint32_t>(points.size());
        points.push_back(p);
        grid[key].push_back(id);
        return id;
    }

    const std::vector<glm::dvec3>& welded() const { return points; }
    std::vector<glm::dvec3> takePoints() { return std::move(points); }

    private:
    struct GridKey {
        int64_t x, y, z;
        bool operator==(const GridKey&) const = default;
    };

    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            const auto u = [](int64_t v) { return static_cast<uint64_t>(v); };
            return std::hash<uint64_t>{}(u(k.x) * 73856093u ^ u(k.y) * 19349663u ^
                                         u(k.z) * 83492791u);
        }
    };

    double eps;
    double cellSize;
    std::vector<glm::dvec3> points;
    std::unordered_map<GridKey, std::vector<uint32_t>, GridKeyHash> grid;

    GridKey keyOf(const glm::dvec3& p) const {
        return {static_cast<int64_t>(std::floor(p.x / cellSize)),
                static_cast<int64_t>(std::floor(p.y / cellSize)),
                static_cast<int64_t>(std::floor(p.z / cellSize))};
    }
};

} // namespace skin::geometry
