#pragma once

#include <chrono>
#include <type_traits>

#include "utility/debug.h"
#include "utility/time.h"

namespace ptgn {

// Monotonic clock to prevent time variations if system time is changed.
// With modifications to: https://gist.github.com/mcleary/b0bf4fa88830ff7c882d
class Timer {
public:
	Timer() = default;
	explicit Timer(bool start);
	~Timer() = default;
	// Acts as a reset.
	void Start();
	void Stop();
	void Pause();
	void Unpause();
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsRunning() const;

	/*
	 * @tparam Duration The unit of time. Default: milliseconds.
	 * @return Elapsed duration of time since timer start.
	 */
	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	[[nodiscard]] Duration Elapsed() const {
		auto end_time = running_ ? std::chrono::steady_clock::now() : stop_time_;
		return std::chrono::duration_cast<Duration>(end_time - start_time_);
	}

	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	[[nodiscard]] bool Completed(Duration compared_to) const {
		return ElapsedPercentage(compared_to) >= 1.0f;
	}

	// @return Elapsed percentage of compared_to time duration. Returns 1.0f if compared_to is 0.
	template <
		typename Duration = milliseconds, typename T = float, tt::duration<Duration> = true,
		std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
	[[nodiscard]] T ElapsedPercentage(Duration compared_to) const {
		if (compared_to == Duration{ 0 }) {
			return 1.0f;
		}
		duration<T, typename Duration::period> elapsed_time{
			Elapsed<duration<T, typename Duration::period>>() / compared_to
		};
		T percentage{ std::clamp(elapsed_time.count(), T{ 0 }, T{ 1 }) };
		PTGN_ASSERT(
			percentage >= T{ 0 } && percentage <= T{ 1 },
			"Elapsed countdown percentage cannot be outside the 0.0 to 1.0 "
			"range"
		);
		return percentage;
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> start_time_;
	std::chrono::time_point<std::chrono::steady_clock> stop_time_;
	std::chrono::time_point<std::chrono::steady_clock> pause_time_;
	bool running_{ false };
	bool paused_{ false };
};

} // namespace ptgn
