#include "skin/geometry/ConvexPolytope.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace skin::geometry {

namespace {

constexpr double RELATIVE_EPSILON = 1e-10;

bool lexLess(const glm::dvec3& a, const glm::dvec3& b) {
    if (a.x != b.x) {
        return a.x < b.x;
    }
    if (a.y != b.y) {
        return a.y < b.y;
    }
    return a.z < b.z;
}

// Endpoints in a fixed order, so both faces sharing an edge get the same point.
glm::dvec3 crossing(glm::dvec3 a, double da, glm::dvec3 b, double db) {
    if (lexLess(b, a)) {
        std::swap(a, b);
        std::swap(da, db);
    }
    return a + (da / (da - db)) * (b - a);
}

bool coincident(const glm::dvec3& a, const glm::dvec3& b, double eps) {
    const glm::dvec3 d = glm::abs(a - b);
    return std::max({d.x, d.y, d.z}) <= eps;
}

void removeRepeats(std::vector<glm::dvec3>& ring, double eps) {
    std::vector<glm::dvec3> out;
    out.reserve(ring.size());
    for (const glm::dvec3& p : ring) {
        if (out.empty() || !coincident(out.back(), p, eps)) {
            out.push_back(p);
        }
    }
    while (out.size() > 1 && coincident(out.front(), out.back(), eps)) {
        out.pop_back();
    }
    ring = std::move(out);
}

// Points on the cutting plane, ordered counter-clockwise about n.
std::vector<glm::dvec3> orderCap(std::vector<glm::dvec3> points, const glm::dvec3& n,
                                 double eps) {
    std::vector<glm::dvec3> unique;
    for (const glm::dvec3& p : points) {
        const bool seen = std::any_of(unique.begin(), unique.end(),
                                      [&](const glm::dvec3& q) { return coincident(p, q, eps); });
        if (!seen) {
            unique.push_back(p);
        }
    }
    if (unique.size() < 3) {
        return {};
    }

    glm::dvec3 centre(0.0);
    for (const glm::dvec3& p : unique) {
        centre += p;
    }
    centre /= static_cast<double>(unique.size());

    const glm::dvec3 helper = std::abs(n.x) < 0.9 ? glm::dvec3(1, 0, 0) : glm::dvec3(0, 1, 0);
    const glm::dvec3 u = glm::normalize(glm::cross(n, helper));
    const glm::dvec3 v = glm::cross(n, u);
    std::vector<std::pair<double, glm::dvec3>> keyed;
    keyed.reserve(unique.size());
    for (const glm::dvec3& p : unique) {
        const glm::dvec3 r = p - centre;
        keyed.emplace_back(std::atan2(glm::dot(r, v), glm::dot(r, u)), p);
    }
    std::sort(keyed.begin(), keyed.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<glm::dvec3> ring;
    ring.reserve(keyed.size());
    for (const auto& entry : keyed) {
        ring.push_back(entry.second);
    }
    return ring;
}

} // namespace

ConvexPolytope ConvexPolytope::fromBox(const Aabb& box) {
    const glm::dvec3& a = box.min;
    const glm::dvec3& b = box.max;
    ConvexPolytope result;
    result.faces = {
        {.ring = {{a.x, a.y, a.z}, {a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}}},
        {.ring = {{b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {b.x, a.y, b.z}}},
        {.ring = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}}},
        {.ring = {{a.x, b.y, a.z}, {a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}}},
        {.ring = {{a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, a.y, a.z}}},
        {.ring = {{a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}}},
    };
    return result;
}

ConvexPolytope ConvexPolytope::fromPolygons(std::vector<std::vector<glm::dvec3>> polygons) {
    ConvexPolytope result;
    result.faces.reserve(polygons.size());
    for (auto& polygon : polygons) {
        result.faces.push_back({.ring = std::move(polygon)});
    }
    return result;
}

bool ConvexPolytope::clip(const glm::dvec3& normal, double offset, uint32_t tag) {
    const double length = glm::length(normal);
    if (faces.empty() || length == 0.0) {
        return false;
    }
    const glm::dvec3 n = normal / length;
    const double d = offset / length;

    double extent = std::abs(d);
    for (const Face& face : faces) {
        for (const glm::dvec3& p : face.ring) {
            const glm::dvec3 a = glm::abs(p);
            extent = std::max({extent, a.x, a.y, a.z});
        }
    }
    const double eps = RELATIVE_EPSILON * (1.0 + extent);

    // Snapped to exactly 0 within eps, so "on the plane" is decided once.
    const auto signedDistance = [&](const glm::dvec3& p) {
        const double distance = glm::dot(n, p) - d;
        return std::abs(distance) <= eps ? 0.0 : distance;
    };

    bool anyOutside = false;
    bool anyInside = false;
    for (const Face& face : faces) {
        for (const glm::dvec3& p : face.ring) {
            const double distance = signedDistance(p);
            anyOutside = anyOutside || distance > 0.0;
            anyInside = anyInside || distance < 0.0;
        }
    }
    if (!anyOutside) {
        return false;
    }
    if (!anyInside) {
        faces.clear();
        return true;
    }

    // Sutherland-Hodgman per face; the points on the plane close the cut.
    std::vector<Face> kept;
    kept.reserve(faces.size() + 1);
    std::vector<glm::dvec3> capPoints;
    bool faceOnPlane = false;
    for (const Face& face : faces) {
        Face out{.ring = {}, .tag = face.tag};
        bool allOnPlane = true;
        const size_t count = face.ring.size();
        for (size_t k = 0; k < count; ++k) {
            const glm::dvec3& a = face.ring[k];
            const glm::dvec3& b = face.ring[(k + 1) % count];
            const double da = signedDistance(a);
            const double db = signedDistance(b);
            allOnPlane = allOnPlane && da == 0.0;
            if (da <= 0.0) {
                out.ring.push_back(a);
                if (da == 0.0) {
                    capPoints.push_back(a);
                }
            }
            if ((da < 0.0 && db > 0.0) || (da > 0.0 && db < 0.0)) {
                const glm::dvec3 p = crossing(a, da, b, db);
                out.ring.push_back(p);
                capPoints.push_back(p);
            }
        }
        faceOnPlane = faceOnPlane || allOnPlane;
        removeRepeats(out.ring, eps);
        if (out.ring.size() >= 3) {
            kept.push_back(std::move(out));
        }
    }

    if (!faceOnPlane) {
        std::vector<glm::dvec3> cap = orderCap(std::move(capPoints), n, eps);
        if (cap.size() >= 3) {
            kept.push_back({.ring = std::move(cap), .tag = tag});
        }
    }

    faces = std::move(kept);
    if (faces.size() < 4) {
        faces.clear();
    }
    return true;
}

bool ConvexPolytope::clip(const Aabb& box) {
    bool cut = false;
    for (int axis = 0; axis < 3; ++axis) {
        glm::dvec3 n(0.0);
        n[axis] = 1.0;
        cut = clip(n, box.max[axis]) || cut;
        cut = clip(-n, -box.min[axis]) || cut;
    }
    return cut;
}

const ConvexPolytope::Face* ConvexPolytope::findFace(uint32_t tag) const {
    const auto it =
        std::find_if(faces.begin(), faces.end(), [&](const Face& f) { return f.tag == tag; });
    return it == faces.end() ? nullptr : &*it;
}

glm::dvec3 ConvexPolytope::vertexMean() const {
    glm::dvec3 sum(0.0);
    size_t count = 0;
    for (const Face& face : faces) {
        for (const glm::dvec3& p : face.ring) {
            sum += p;
            ++count;
        }
    }
    return count == 0 ? sum : sum / static_cast<double>(count);
}

// Fan of tets from an interior point; |.| per tet makes face orientation
// irrelevant.
double ConvexPolytope::volume() const {
    const glm::dvec3 c = vertexMean();
    double total = 0.0;
    for (const Face& face : faces) {
        const glm::dvec3 r0 = face.ring[0] - c;
        for (size_t k = 1; k + 1 < face.ring.size(); ++k) {
            const glm::dvec3 r1 = face.ring[k] - c;
            const glm::dvec3 r2 = face.ring[k + 1] - c;
            total += std::abs(glm::dot(r0, glm::cross(r1, r2)));
        }
    }
    return total / 6.0;
}

} // namespace skin::geometry
