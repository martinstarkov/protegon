#pragma once

#include <chrono> // std::chrono
#include <cstdint> // std::int64_t

namespace engine {

// Monotonic clock to prevent time variations if system time is changed.
// With modifications to: https://gist.github.com/mcleary/b0bf4fa88830ff7c882d

class Timer {
public:
    Timer() = default;
	~Timer() = default;
    // This works if wanting to restart an active timer.
    void Start() {
        start_time_ = std::chrono::steady_clock::now();
        running_ = true;
    }
    void Stop() {
        stop_time_ = std::chrono::steady_clock::now();
        running_ = false;
    }
    std::int64_t ElapsedMilliseconds() {
        std::chrono::time_point<std::chrono::steady_clock> end_time;

        if (running_) {
            end_time = std::chrono::steady_clock::now();
        } else {
            end_time = stop_time_;
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_).count();
    }
    double ElapsedSeconds() {
        return ElapsedMilliseconds() / 1000.0;
    }
private:
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::chrono::time_point<std::chrono::steady_clock> stop_time_;
    bool running_ = false;
};

}