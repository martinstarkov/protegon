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

bool Timer::IsRunning() const {
    return running_;
}

void Timer::Reset() {
    stop_time_ = start_time_;
    running_ = false;
}

} // namespace ptgn
