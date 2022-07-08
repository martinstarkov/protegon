#pragma once

#include <chrono> // std::chrono

#include "utility/TypeTraits.h"

namespace ptgn {

using hours = std::chrono::hours;
using minutes = std::chrono::minutes;
using seconds = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using nanoseconds = std::chrono::nanoseconds;

// Monotonic clock to prevent time variations if system time is changed.
// With modifications to: https://gist.github.com/mcleary/b0bf4fa88830ff7c882d
class Timer {
public:
    Timer(bool start = false) {
        if (start) {
            Start();
        }
    }
	~Timer() = default;
    /*
    * Start the timer. Acts as a reset.
    */
    void Start() {
        start_time_ = std::chrono::steady_clock::now();
        running_ = true;
    }
    /*
    * Stop the timer.
    */
    void Stop() {
        stop_time_ = std::chrono::steady_clock::now();
        running_ = false;
    }
    /*
    * @return True if timer is running, false otherwise.
    */
    bool IsRunning() const {
        return running_;
    }
    /*
    * Reset timer to zero elapsed time.
    */
    // This does not actually start the timer, just sets it to original configuration.
    // TODO: POSSIBLE CHANGE: Change this to start the timer?
    void Reset() {
        stop_time_ = start_time_;
        running_ = false;
    }
    /*
    * @tparam Duration The unit of time. Default: milliseconds.
    * @return Elapsed duration of time since timer start.
    */
    template <typename Duration = milliseconds, 
        type_traits::is_duration_e<Duration> = true>
    Duration Elapsed() const {
        std::chrono::time_point<std::chrono::steady_clock> end_time;
        if (running_) {
            end_time = std::chrono::steady_clock::now();
        } else {
            end_time = stop_time_;
        }
        return std::chrono::duration_cast<Duration>(end_time - start_time_);
    }
private:
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::chrono::time_point<std::chrono::steady_clock> stop_time_;
    bool running_{ false };
};

} // namespace ptgn