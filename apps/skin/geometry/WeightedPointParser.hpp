#pragma once

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <json.hpp>

#include "skin/geometry/WeightedPoint.hpp"

namespace skin::geometry {

class WeightedPointParser {
    public:
    static std::vector<WeightedPoint> parseFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("WeightedPointParser: failed to open file: " + path);
        }
        return parseStream(file, path);
    }

    static std::vector<WeightedPoint> parseStream(std::istream& stream,
                                                  const std::string& sourceName = "<stream>") {
        nlohmann::json root;
        try {
            stream >> root;
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error("WeightedPointParser: JSON parse error in " + sourceName +
                                     ": " + e.what());
        }

        if (!root.is_array()) {
            throw std::runtime_error("WeightedPointParser: expected top-level JSON array in " +
                                     sourceName);
        }

        std::vector<WeightedPoint> points;
        points.reserve(root.size());

        for (std::size_t i = 0; i < root.size(); ++i) {
            const auto& entry = root[i];

            if (!entry.is_object()) {
                throw std::runtime_error("WeightedPointParser: element " + std::to_string(i) +
                                         " in " + sourceName + " is not an object");
            }

            for (const char* field : {"x", "y", "z", "weight"}) {
                if (!entry.contains(field)) {
                    throw std::runtime_error("WeightedPointParser: element " + std::to_string(i) +
                                             " in " + sourceName + " missing field \"" + field +
                                             "\"");
                }
                if (!entry[field].is_number()) {
                    throw std::runtime_error("WeightedPointParser: element " + std::to_string(i) +
                                             " in " + sourceName + " field \"" + field +
                                             "\" is not numeric");
                }
            }

            // Read as double: get<float>() on a value beyond FLT_MAX is UB, and
            // non-finite coordinates break CGAL's exact arithmetic.
            double values[4];
            const char* fields[4] = {"x", "y", "z", "weight"};
            for (int k = 0; k < 4; ++k) {
                values[k] = entry[fields[k]].get<double>();
                const double limit =
                    k < 3 ? static_cast<double>(std::numeric_limits<float>::max())
                          : std::numeric_limits<double>::max();
                if (!std::isfinite(values[k]) || std::abs(values[k]) > limit) {
                    throw std::runtime_error("WeightedPointParser: element " + std::to_string(i) +
                                             " in " + sourceName + " field \"" + fields[k] +
                                             "\" is out of range");
                }
            }

            points.push_back(WeightedPoint{glm::vec3(static_cast<float>(values[0]),
                                                     static_cast<float>(values[1]),
                                                     static_cast<float>(values[2])),
                                           values[3]});
        }

        return points;
    }
};

} // namespace skin::geometry
