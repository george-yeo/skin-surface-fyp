#pragma once

#include <format>
#include <print>
#include <stdexcept>
#include <string>

namespace nacre::core {

// Thrown by Logger::fatal once the message has been logged; main() catches it.
class FatalError : public std::runtime_error {
    public:
    explicit FatalError(const std::string& message) : std::runtime_error(message) {}
};

class Logger {
    public:
    Logger() : name("Unknown") {};
    Logger(const std::string& name) : name(name) {};

    template <typename... Args>
    void print(std::format_string<Args...> fmt, Args&&... args) const {
        if (LogLevel::Debug >= logLevel) {
            std::println("{}[DEBUG]{} {}[{}]{} {}", CYAN, RESET, GREY, name, RESET,
                         std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) const {
        if (LogLevel::Warning >= logLevel) {
            std::println("{}[WARN]{} {}[{}]{} {}", YELLOW, RESET, GREY, name, RESET,
                         std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) const {
        if (LogLevel::Error >= logLevel) {
            std::println(stderr, "{}[ERROR]{} {}[{}]{} {}", RED, RESET, GREY, name, RESET,
                         std::format(fmt, std::forward<Args>(args)...));
        }
    }

    template <typename... Args>
    [[noreturn]] void fatal(std::format_string<Args...> fmt, Args&&... args) const {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        std::println(stderr, "{}[FATAL]{} {}[{}]{} {}", BOLD_RED, RESET, GREY, name, RESET, msg);
        throw FatalError(msg);
    }

    template <typename... Args>
    void check(bool condition, std::format_string<Args...> fmt, Args&&... args) const {
        if (!condition) {
            // Forward to fatal to log and stop execution
            fatal(fmt, std::forward<Args>(args)...);
        }
    }

    private:
    enum LogLevel : int {
        None,
        Debug,
        Warning,
        Error,
    };

    const std::string name;
    // Messages at or above this level are printed.
    const LogLevel logLevel = LogLevel::Debug;

    // ANSI color codes
    static constexpr const char* RESET = "\033[0m";
    static constexpr const char* CYAN = "\033[36m";
    static constexpr const char* YELLOW = "\033[33m";
    static constexpr const char* RED = "\033[31m";
    static constexpr const char* BOLD_RED = "\033[1;31m";
    static constexpr const char* GREY = "\033[90m";
};

} // namespace nacre::core
