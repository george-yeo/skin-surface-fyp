#pragma once

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <nacre/core/Logger.hpp>
#include "skin/geometry/SkinBuild.hpp"

namespace skin {

// Runs one meshing job at a time on its own thread, without ever blocking the
// caller: starting a job while one runs asks the running one to stop and
// queues the new one (only the latest is kept). Results of stopped jobs are
// dropped. All methods are for the owning thread.
template <class Result>
class BackgroundTask {
    public:
    using Job = std::function<Result(geometry::MeshingControl&)>;

    BackgroundTask() = default;
    BackgroundTask(const BackgroundTask&) = delete;
    BackgroundTask& operator=(const BackgroundTask&) = delete;

    void start(Job job) {
        pending = std::move(job);
        if (worker.joinable()) {
            worker.request_stop();
        } else {
            launch();
        }
    }

    // Drops the queued job and asks the running one to stop.
    void cancel() {
        pending.reset();
        if (worker.joinable()) {
            worker.request_stop();
        }
    }

    // Call once per frame. Returns the result of a job that finished without
    // being stopped, then launches the queued job, if any.
    std::optional<Result> poll() {
        if (!worker.joinable() || !finished.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        const bool stopped = worker.get_stop_token().stop_requested();
        worker.join();

        std::optional<Result> result;
        if (failure) {
            logger.error("Meshing job failed: {}", *failure);
        } else if (!stopped) {
            result = std::move(output);
        }
        output.reset();
        failure.reset();
        if (pending) {
            launch();
        }
        return result;
    }

    geometry::MeshingStatus status() const {
        if (!worker.joinable()) {
            return {};
        }
        return {.busy = true,
                .phase = control.phase.load(),
                .fraction = control.fraction.load(),
                .tets = control.tets.load(),
                .seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                                         started)
                               .count()};
    }

    private:
    nacre::core::Logger logger{"BackgroundTask"};
    geometry::MeshingControl control;
    std::optional<Job> pending;
    // Written by the worker before it sets `finished`.
    std::optional<Result> output;
    std::optional<std::string> failure;
    std::atomic<bool> finished{false};
    std::chrono::steady_clock::time_point started;
    // Last: destroyed first, so its destructor (stop, join) runs while the
    // members the worker writes are still alive.
    std::jthread worker;

    void launch() {
        Job job = std::move(*pending);
        pending.reset();
        control.phase = geometry::MeshingPhase::Starting;
        control.fraction = -1.0f;
        control.tets = 0;
        finished.store(false);
        started = std::chrono::steady_clock::now();
        worker = std::jthread([this, job = std::move(job)](std::stop_token stop) {
            control.stop = std::move(stop);
            try {
                output = job(control);
            } catch (const std::exception& e) {
                failure = e.what();
            } catch (...) {
                failure = "unknown exception";
            }
            finished.store(true, std::memory_order_release);
        });
    }
};

} // namespace skin
