#pragma once

#include <chrono>

namespace nacre::core {

class Timer {
    public:
    void start() { startTime = std::chrono::high_resolution_clock::now(); }
    // Fractional milliseconds. (A duration_cast to whole milliseconds used to
    // truncate any sub-millisecond frame to 0.)
    std::chrono::duration<double, std::milli> getElapsedTime() {
        return std::chrono::high_resolution_clock::now() - startTime;
    }
    auto restart() {
        auto elapsed = getElapsedTime();
        start();
        return elapsed;
    }

    private:
    std::chrono::high_resolution_clock::time_point startTime;
};

} // namespace nacre::core
