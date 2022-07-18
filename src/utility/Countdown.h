#pragma once

#include <cassert> // assert

#include "utility/Timer.h"

namespace ptgn {

class Countdown {
public:
	using time = milliseconds;
	Countdown(bool start = false) {
		if (start) {
			Start();
		}
	}
	~Countdown() = default;

	template <typename Duration = time,
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

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	Duration Elapsed() const {
		return timer_.Elapsed<Duration>();
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	Duration Remaining() const {
		time remaining{ cutoff_ - timer_.Elapsed<time>() };
		if (remaining > time{ 0 }) {
			return std::chrono::duration_cast<Duration>(remaining);
		}
		return Duration{ 0 };
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	void IncreaseRemaining(Duration time_amount) {
		cutoff_ += time_amount;
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	void DecreaseRemaining(Duration time_amount) {
		cutoff_ -= time_amount;
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	void SetRemaining(Duration time_amount) {
		cutoff_ = time_amount;
	}
private:
	time cutoff_{ 0 };
	Timer timer_;
};

class FrameCountdown {
public:
	using time = std::chrono::duration<double>;
	FrameCountdown(bool start = false) {
		if (start) {
			Start();
		}
	}
	~FrameCountdown() = default;
	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	FrameCountdown(Duration time_remaining, bool start = false) : remaining_{ std::chrono::duration_cast<time>(time_remaining) }, original_remaining_{ remaining_ } {
		if (start) {
			Start();
		}
	}
	void Start() {
		running_ = true;
	}
	void Stop() {
		running_ = true;
	}
	void Reset() {
		remaining_ = original_remaining_;
		running_ = false;
	}
	bool Finished() const {
		return remaining_ <= time{ 0 };
	}
	bool IsRunning() const {
		return running_ && !Finished();
	}
	template <typename T = double,
		std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
	T RemainingPercentage() const {
		std::chrono::duration<T, time::period> percentage_time{ remaining_ / original_remaining_ };
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

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	Duration Elapsed() const {
		return std::chrono::duration_cast<Duration>(original_remaining_ - remaining_);
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	Duration Remaining() const {
		if (remaining_ > time{ 0 }) {
			return std::chrono::duration_cast<Duration>(remaining_);
		}
		return Duration{ 0 };
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	void IncreaseRemaining(Duration time_amount) {
		remaining_ += std::chrono::duration_cast<time>(time_amount);
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	void DecreaseRemaining(Duration time_amount) {
		remaining_ -= std::chrono::duration_cast<time>(time_amount);
	}

	template <typename Duration = time,
		type_traits::is_duration_e<Duration> = true>
	void SetRemaining(Duration time_amount) {
		remaining_ = std::chrono::duration_cast<time>(time_amount);
	}

	void Update(double dt) {
		if (IsRunning()) {
			std::chrono::duration<double> subtract{ dt };
			remaining_ -= subtract;
		}
	}
private:
	time remaining_{ 0 };
	time original_remaining_{ 0 };
	bool running_{ false };
};

} // namespace ptgn