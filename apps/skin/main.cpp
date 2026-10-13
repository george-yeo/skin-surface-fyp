#include "skin/SkinApp.hpp"

#include <cstdio>
#include <exception>
#include <print>

#include <nacre/core/Logger.hpp>

// Catching here (rather than letting exceptions escape main) guarantees the
// stack unwinds, so the app's destructors tear Vulkan down cleanly.
int main() {
    try {
        skin::SkinApp app;
        app.run();
    } catch (const nacre::core::FatalError&) {
        return 1; // already logged by Logger::fatal
    } catch (const std::exception& e) {
        std::println(stderr, "Unhandled exception: {}", e.what());
        return 1;
    }

    return 0;
}