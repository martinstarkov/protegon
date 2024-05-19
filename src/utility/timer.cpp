#include "protegon/timer.h"

namespace ptgn {

Timer::Timer(bool start) {
    if (start) {
        Start();
    }
}

void Timer::Start() {
    start_time_ = std::chrono::steady_clock::now();
    running_ = true;
}

void Timer::Stop() {
    stop_time_ = std::chrono::steady_clock::now();
    running_ = false;
}

void Timer::Pause() {
    if (running_) {
        stop_time_ = std::chrono::steady_clock::now();
        pause_time_ = std::chrono::steady_clock::now();
        running_ = false;
    }
}

void Timer::Unpause() {
    if (!running_) {
        // Calculate elapsed time during pause.
        auto pause_duration = std::chrono::steady_clock::now() - pause_time_;
        // Adjust start time to account for pause.
        start_time_ += pause_duration;
        running_ = true;
        pause_time_ = std::chrono::steady_clock::time_point(); // Reset paused time on unpause
        stop_time_ = start_time_;
    }
}

bool Timer::IsRunning() const {
    return running_;
}

void Timer::Reset() {
    stop_time_ = start_time_;
    running_ = false;
}

} // namespace ptgn
