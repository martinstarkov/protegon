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

    template <typename Duration = milliseconds, 
        typename T = float,
        type_traits::duration<Duration> = true,
        std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
    T ElapsedPercentage(Duration compared_to) const {
        std::chrono::duration<T, Duration::period> elapsed_time{
            Elapsed<std::chrono::duration<T, Duration::period>>() / compared_to
        };
        T percentage{ std::clamp(elapsed_time.count(), static_cast<T>(0), static_cast<T>(1)) };
        assert(percentage >= static_cast<T>(0) &&
               percentage <= static_cast<T>(1) &&
               "Elapsed countdown percentage cannot be outside the 0.0 to 1.0 range");
        return percentage;
    }
private:
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    std::chrono::time_point<std::chrono::steady_clock> stop_time_;
    bool running_{ false };
};

} // namespace ptgn