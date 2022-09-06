#pragma once

#include "time.h"

namespace ptgn {

// Monotonic clock to prevent time variations if system time is changed.
// With modifications to: https://gist.github.com/mcleary/b0bf4fa88830ff7c882d
class Timer {
public:
    Timer(bool start = false);
	~Timer() = default;
    // Acts as a reset.
    void Start();
    void Stop();
    bool IsRunning() const;
    // This does not actually start the timer, just sets it to original configuration.
    // TODO: POSSIBLE CHANGE: Change this to start the timer?
    void Reset();
    /*
    * @tparam Duration The unit of time. Default: milliseconds.
    * @return Elapsed duration of time since timer start.
    */
    template <typename Duration = milliseconds, 
        type_traits::duration<Duration> = true>
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