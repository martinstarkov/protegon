#pragma once

#include <cassert> // assert

#include "utility/Timer.h"

namespace ptgn {

class Countdown {
public:
	Countdown(bool start = false) {
		if (start) {
			Start();
		}
	}
	~Countdown() = default;

	template <typename Duration,
		type_traits::is_duration_e<Duration> = true>
	Countdown(Duration time_remaining, bool start = false) : cutoff_{ time_remaining } {
		if (start) {
			Start();
		}
	}

	void Start() {
		timer_.Start();
	}

	void Stop() {
		timer_.Stop();
	}

	void Reset() {
		timer_.Reset();
	}

	bool IsRunning() const {
		return timer_.IsRunning() && !Finished();
	}

	bool Finished() const {
		return Remaining<time>() <= time{ 0 };
	}

	template <typename T = double, 
		std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
	T RemainingPercentage() const {
		std::chrono::duration<T, time::period> percentage_time{ 
			Remaining<std::chrono::duration<T, time::period>>() / cutoff_
		};
		T percentage{ percentage_time.count() };
		assert(percentage >= static_cast<T>(0) &&
			   percentage <= static_cast<T>(1) &&
			   "Remaining countdown percentage cannot be outside the 0.0 to 1.0 range");
		return percentage;
	}

	template <typename T = double,
		std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
	T ElapsedPercentage() const {
		return static_cast<T>(1) - RemainingPercentage<T>();
	}

	template <typename Duration = milliseconds,
		type_traits::is_duration_e<Duration> = true>
	Duration Elapsed() const {
		return timer_.Elapsed<Duration>();
	}

	template <typename Duration = milliseconds,
		type_traits::is_duration_e<Duration> = true>
	Duration Remaining() const {
		time remaining{ cutoff_ - timer_.Elapsed<time>() };
		if (remaining > time{ 0 }) {
			return std::chrono::duration_cast<Duration>(remaining);
		}
		return Duration{ 0 };
	}

	template <typename Duration = milliseconds,
		type_traits::is_duration_e<Duration> = true>
	void IncreaseRemaining(Duration time_amount) {
		cutoff_ += time_amount;
	}

	template <typename Duration = milliseconds,
		type_traits::is_duration_e<Duration> = true>
	void DecreaseRemaining(Duration time_amount) {
		cutoff_ -= time_amount;
	}

	template <typename Duration = milliseconds,
		type_traits::is_duration_e<Duration> = true>
	void SetRemaining(Duration time_amount) {
		cutoff_ = time_amount;
	}
private:
	using time = nanoseconds;
	time cutoff_{ 0 };
	Timer timer_;
};

} // namespace ptgn