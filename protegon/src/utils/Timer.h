#pragma once

#include <chrono> // std::chrono
#include <ratio> // std::milli, etc
#include <cstdint> // std::int64_t

namespace engine {

using seconds = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using nanoseconds = std::chrono::nanoseconds;

namespace type_traits {

template <typename T>
struct is_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename T>
constexpr bool is_duration_v{ is_duration<T>::value };

template <typename T>
using is_duration_e = std::enable_if_t<is_duration_v<T>, bool>;

} // namespace type_traits

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
    bool IsRunning() const {
        return running_;
    }
    void Reset() {
        stop_time_ = start_time_;
        running_ = false;
    }
    template <typename Duration, 
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

} // namespace engine