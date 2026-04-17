#pragma once

#include <chrono>
#include <ostream>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/util/time.h"
#include "serialization/json/json.h"

namespace ptgn {

/// @brief Monotonic clock to prevent time variations if system time is changed.
class Timer {
public:
	Timer() = default;

	/// @param start Whether to start the timer immediately upon construction or not.
	explicit Timer(bool start) {
		if (start) {
			Start();
		}
	}

	/// @brief Starts the timer. Can also be used to restart the timer.
	/// @param force If false, only starts the timer if it is not already running.
	/// @return True if the timer is newly started, false if it was already running.
	bool Start(bool force = true) {
		if (!force && IsRunning()) {
			return false;
		}
		start_time_ = std::chrono::steady_clock::now();
		running_	= true;
		paused_		= false;
		return true;
	}

	/// @brief Stops and resets the timer.
	void Reset() {
		start_time_ = std::chrono::steady_clock::now();
		pause_time_ = std::chrono::steady_clock::now();
		offset_		= std::chrono::steady_clock::duration::zero();
		Stop();
	}

	void Stop() {
		stop_time_ = std::chrono::steady_clock::now();
		running_   = false;
		paused_	   = false;
	}

	/// @brief Toggles the pause state of the timer.
	void Toggle() {
		if (IsRunning()) {
			Stop();
		} else {
			Start();
		}
	}

	void Pause() {
		if (running_ && !paused_) {
			stop_time_	= std::chrono::steady_clock::now();
			pause_time_ = std::chrono::steady_clock::now();
			running_	= false;
			paused_		= true;
		}
	}

	void Resume() {
		if (!running_ && paused_) {
			// Calculate elapsed time during pause.
			auto pause_duration = std::chrono::steady_clock::now() - pause_time_;
			// Adjust start time to account for pause.
			start_time_ += pause_duration;
			running_	 = true;
			paused_		 = false;
			pause_time_	 = std::chrono::steady_clock::time_point(); // Reset paused time on unpause
			stop_time_	 = start_time_;
		}
	}

	/// @return True if the timer is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused() const {
		return paused_;
	}

	/// @return True if the timer is currently running, false otherwise.
	[[nodiscard]] bool IsRunning() const {
		return running_;
	}

	/// @return True if the timer has run before (not necessarily now) without being reset, false
	/// otherwise.
	[[nodiscard]] bool HasRun() const {
		return start_time_ != stop_time_;
	}

	/// @tparam Duration The unit of time. Default: milliseconds.
	/// @param Amount of time to add to the timer.
	template <DurationType D = milliseconds>
	void AddOffset(D extra_time) {
		offset_ += extra_time;
	}

	/// @tparam Duration The unit of time. Default: milliseconds.
	/// @param Amount of time to remove from the timer.
	template <DurationType D = milliseconds>
	void RemoveOffset(D time_to_remove) {
		offset_ -= time_to_remove;
	}

	/// @tparam Duration The unit of time. Default: milliseconds.
	/// @return Elapsed duration of time since timer start.
	template <DurationType D = milliseconds>
	[[nodiscard]] D ElapsedDuration() const {
		auto end_time = running_ ? std::chrono::steady_clock::now() : stop_time_;
		return duration_cast<D>(end_time - start_time_ + offset_);
	}

	/// @tparam Duration The unit of time. Default: milliseconds.
	/// @param compared_to The time to check that the timer has completed.
	/// @return True the timer has elapsed compared_to time and false if not.
	template <DurationType D = milliseconds>
	[[nodiscard]] bool Completed(D compared_to) const {
		return ElapsedFraction(compared_to) >= 1.0f;
	}

	/// @tparam Duration The unit of time. Default: milliseconds.
	/// @param duration The time relative to which the elapsed time is returned.
	/// @return Elapsed fraction of compared_to time duration clamped to range [0.0, 1.0]. Returns
	/// 1 if duration is 0.
	template <DurationType D = milliseconds>
	[[nodiscard]] float ElapsedFraction(D duration) const {
		if (duration == D{ 0 }) {
			return 1.0f;
		}
		using T = ptgn::duration<float, typename D::period>;
		T elapsed_time{ ElapsedDuration<T>() / duration };
		float elapsed{ Clamp01(elapsed_time.count()) };
		return elapsed;
	}

	bool operator==(const Timer&) const = default;

	friend void to_json(json& j, const Timer& timer) {
		j["running"] = timer.running_;
		j["paused"]	 = timer.paused_;
	}

	friend void from_json(const json& j, Timer& timer) {
		j.at("running").get_to(timer.running_);
		j.at("paused").get_to(timer.paused_);
		if (timer.running_) {
			timer.Start(true);
		} else {
			timer.Stop();
		}
		if (timer.paused_) {
			timer.Pause();
		} else {
			timer.Resume();
		}
	}

	friend std::ostream& operator<<(std::ostream& os, const Timer& timer) {
		return os << "{ running: " << timer.running_ << ", paused: " << timer.paused_
				  << ", elapsed: " << timer.ElapsedDuration<secondsf>() << "s }";
	}

private:
	std::chrono::time_point<std::chrono::steady_clock> start_time_{};
	std::chrono::time_point<std::chrono::steady_clock> stop_time_{};
	std::chrono::time_point<std::chrono::steady_clock> pause_time_{};
	std::chrono::steady_clock::duration offset_{};

	bool running_{ false };
	bool paused_{ false };
};

} // namespace ptgn