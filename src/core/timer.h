#pragma once

#include <chrono>
#include <type_traits>

#include "common/assert.h"
#include "core/time.h"
#include "serialization/fwd.h"

namespace ptgn {

class Scene;

// Monotonic clock to prevent time variations if system time is changed.
class Timer {
public:
	Timer() = default;

	// @param start Whether to start the timer immediately upon construction or not.
	explicit Timer(bool start);

	// Starts the timer. Can also be used to restart the timer.
	// @param force If false, only starts the timer if it is not already running.
	// @return True if the timer is newly started, false if it was already running.
	bool Start(bool force = true);

	// Stops and resets the timer.
	void Reset();

	void Stop();

	// Toggles the pause state of the timer.
	void Toggle();

	void Pause();

	void Resume();

	// @return True if the timer is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused() const;

	// @return True if the timer is currently running, false otherwise.
	[[nodiscard]] bool IsRunning() const;

	/*
	 * @tparam Duration The unit of time. Default: milliseconds.
	 * @param Amount of time to add to the timer.
	 */
	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	void AddOffset(Duration extra_time) {
		offset_ += extra_time;
	}

	/*
	 * @tparam Duration The unit of time. Default: milliseconds.
	 * @param Amount of time to remove from the timer.
	 */
	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	void RemoveOffset(Duration time_to_remove) {
		offset_ -= time_to_remove;
	}

	/*
	 * @tparam Duration The unit of time. Default: milliseconds.
	 * @return Elapsed duration of time since timer start.
	 */
	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	[[nodiscard]] Duration Elapsed() const {
		auto end_time = running_ ? std::chrono::steady_clock::now() : stop_time_;
		return std::chrono::duration_cast<Duration>(end_time - start_time_ + offset_);
	}

	/*
	 * @tparam Duration The unit of time. Default: milliseconds.
	 * @param compared_to The time to check that the timer has completed.
	 * @return True the timer has elapsed compared_to time and false if not.
	 */
	template <typename Duration = milliseconds, tt::duration<Duration> = true>
	[[nodiscard]] bool Completed(Duration compared_to) const {
		return ElapsedPercentage(compared_to) >= 1.0f;
	}

	/*
	 * @tparam Duration The unit of time. Default: milliseconds.
	 * @param compared_to The time relative to which the elapsed time is returned.
	 * @return Elapsed percentage of compared_to time duration clamped between 0.0 and 1.0. Returns
	 * 1 if compared_to is 0.
	 */
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

	friend void to_json(json& j, const Timer& timer);
	friend void from_json(const json& j, Timer& timer);

private:
	std::chrono::time_point<std::chrono::steady_clock> start_time_{};
	std::chrono::time_point<std::chrono::steady_clock> stop_time_{};
	std::chrono::time_point<std::chrono::steady_clock> pause_time_{};
	std::chrono::steady_clock::duration offset_{};

	bool running_{ false };
	bool paused_{ false };
};

namespace impl {

struct TimerInfo {
	Timer timer;
	// Duration of the timer.
	milliseconds duration{ 0 };
};

class ScriptTimers {
public:
	static void Update(Scene& scene);

	std::unordered_map<std::size_t, TimerInfo> timers;
};

struct RepeatInfo {
	Timer timer;
	// delay to next execution.
	milliseconds delay{ 0 };
	// current number of executions (first value passed to OnRepeatUpdate is 0).
	int current_executions{ 0 };
	// -1 for infinite executions.
	int max_executions{ 0 };
};

class ScriptRepeats {
public:
	static void Update(Scene& scene);

	std::unordered_map<std::size_t, RepeatInfo> repeats;
};

} // namespace impl

} // namespace ptgn
