#pragma once

#include <cstdint>

#include <nacre/platform/fwd.hpp>

namespace nacre::platform {

// Owns one key or mouse binding: destroying or reassigning it unbinds. Hold it
// in the object the bound callback captures, so the callback can't outlive it.
// Must not outlive the InputManager that issued it. Empty when binding failed.
class [[nodiscard]] InputBinding {
    public:
    InputBinding() = default;
    ~InputBinding();

    InputBinding(const InputBinding&) = delete;
    InputBinding& operator=(const InputBinding&) = delete;

    InputBinding(InputBinding&& other) noexcept;
    InputBinding& operator=(InputBinding&& other) noexcept;

    explicit operator bool() const { return manager != nullptr; }

    private:
    friend class InputManager;
    InputBinding(InputManager* owner, uint64_t bindingId) : manager(owner), id(bindingId) {}

    void release();

    InputManager* manager = nullptr;
    uint64_t id = 0;
};

} // namespace nacre::platform
