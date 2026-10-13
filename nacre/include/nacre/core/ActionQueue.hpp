#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace nacre::core {

class ActionQueue {
    public:
    ActionQueue() = default;
    ~ActionQueue() = default;

    void push(std::function<void()> action, uint32_t delay = 0) {
        actions.push({stepCount + delay, nextSequence++, std::move(action)});
    }
    void execute() {
        while (!actions.empty() && actions.top().due <= stepCount) {
            auto action = actions.top().action;
            actions.pop();
            action();
        }
    }

    void nextFrame() { stepCount++; }

    private:
    struct Entry {
        uint64_t due;
        uint64_t sequence; // FIFO among actions due on the same step
        std::function<void()> action;
    };
    struct Comparator {
        bool operator()(const Entry& a, const Entry& b) const {
            return a.due != b.due ? a.due > b.due : a.sequence > b.sequence;
        }
    };
    std::priority_queue<Entry, std::vector<Entry>, Comparator> actions;
    uint64_t stepCount = 0; // overflow?
    uint64_t nextSequence = 0;
};

} // namespace nacre::core
