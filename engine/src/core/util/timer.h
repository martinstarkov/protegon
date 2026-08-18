#pragma once

#include <chrono>
#include <ostream>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/util/time.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

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
		start_time_ = clock::now();
		running_	= true;
		paused_		= false;
		return true;
	}

	/// @brief Stops and resets the timer.
	void Reset() {
		start_time_ = clock::now();
		pause_time_ = clock::now();
		offset_		= clock_duration::zero();
		Stop();
	}

	void Stop() {
		stop_time_ = clock::now();
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
			stop_time_	= clock::now();
			pause_time_ = clock::now();
			running_	= false;
			paused_		= true;
		}
	}

	void Resume() {
		if (!running_ && paused_) {
			// Calculate elapsed time during pause.
			auto pause_duration = clock::now() - pause_time_;
			// Adjust start time to account for pause.
			start_time_ += pause_duration;
			running_	 = true;
			paused_		 = false;
			pause_time_	 = clock::time_point(); // Reset paused time on unpause
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
		auto end_time = running_ ? clock::now() : stop_time_;
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
	using clock			 = std::chrono::steady_clock;
	using clock_duration = clock::duration;

	std::chrono::time_point<clock> start_time_{};
	std::chrono::time_point<clock> stop_time_{};
	std::chrono::time_point<clock> pause_time_{};
	clock_duration offset_{};

	bool running_{ false };
	bool paused_{ false };
};

/// @brief Timer that only advances when manually updated with dt.
class ManualTimer {
public:
	ManualTimer() = default;

	/// @param start Whether to start the timer immediately upon construction or not.
	explicit ManualTimer(bool start) {
		if (start) {
			Start();
		}
	}

	/// @brief Starts the timer from zero.
	/// @param force If false, does nothing while the timer is already running.
	/// @return True if the timer was started, false if it was already running and force was false.
	bool Start(bool force = false) {
		if (!force && IsRunning()) {
			return false;
		}

		elapsed_ = clock_duration::zero();
		running_ = true;
		paused_ = false;
		has_run_ = true;
		return true;
	}

	/// @brief Restarts the timer from zero.
	bool Restart() {
		return Start(true);
	}

	/// @brief Stops the timer while retaining its elapsed time.
	void Stop() {
		running_ = false;
		paused_ = false;
	}

	/// @brief Stops the timer and resets its elapsed time and run state.
	void Reset() {
		elapsed_ = clock_duration::zero();
		running_ = false;
		paused_ = false;
		has_run_ = false;
	}

	/// @brief Toggles between running and stopped. Starting begins again from zero.
	void Toggle() {
		if (IsRunning()) {
			Stop();
		} else {
			Start();
		}
	}

	void Pause() {
		if (running_ && !paused_) {
			running_ = false;
			paused_ = true;
		}
	}

	void Resume() {
		if (!running_ && paused_) {
			running_ = true;
			paused_ = false;
		}
	}

	/// @brief Advances the timer manually while it is running.
	template <DurationType D = secondsf>
	void Update(D dt) {
		if (!running_ || paused_ || dt <= D{ 0 }) {
			return;
		}

		elapsed_ += duration_cast<clock_duration>(dt);
		has_run_ = true;
	}

	/// @brief Advances elapsed time manually even if the timer is paused or stopped.
	template <DurationType D = secondsf>
	void AddElapsed(D dt) {
		if (dt <= D{ 0 }) {
			return;
		}

		elapsed_ += duration_cast<clock_duration>(dt);
		has_run_ = true;
	}

	/// @brief Removes elapsed time manually, clamped at zero.
	template <DurationType D = secondsf>
	void RemoveElapsed(D dt) {
		if (dt <= D{ 0 }) {
			return;
		}

		elapsed_ -= duration_cast<clock_duration>(dt);

		if (elapsed_ < clock_duration::zero()) {
			elapsed_ = clock_duration::zero();
		}
	}

	/// @brief Compatibility alias for advancing elapsed time.
	template <DurationType D = milliseconds>
	void AddOffset(D extra_time) {
		AddElapsed(extra_time);
	}

	/// @brief Compatibility alias for rewinding elapsed time.
	template <DurationType D = milliseconds>
	void RemoveOffset(D time_to_remove) {
		RemoveElapsed(time_to_remove);
	}

	/// @return True if the timer is currently paused.
	[[nodiscard]] bool IsPaused() const {
		return paused_;
	}

	/// @return True if the timer is currently running.
	[[nodiscard]] bool IsRunning() const {
		return running_;
	}

	/// @return True if the timer has run or been advanced since its last reset.
	[[nodiscard]] bool HasRun() const {
		return has_run_;
	}

	/// @tparam D The unit of time. Default: milliseconds.
	/// @return Elapsed duration since the timer started.
	template <DurationType D = milliseconds>
	[[nodiscard]] D ElapsedDuration() const {
		return duration_cast<D>(elapsed_);
	}

	/// @return True if compared_to has elapsed.
	template <DurationType D = milliseconds>
	[[nodiscard]] bool Completed(D compared_to) const {
		return ElapsedFraction(compared_to) >= 1.0f;
	}

	/// @brief Consumes one completed interval while preserving any remainder.
	template <DurationType D = milliseconds>
	bool Consume(D compared_to) {
		if (compared_to <= D{ 0 } || !Completed(compared_to)) {
			return false;
		}

		const auto amount{ duration_cast<clock_duration>(compared_to) };
		if (amount <= clock_duration::zero()) {
			return false;
		}

		elapsed_ -= amount;
		if (elapsed_ < clock_duration::zero()) {
			elapsed_ = clock_duration::zero();
		}
		return true;
	}

	/// @brief Consumes every completed interval while preserving the final remainder.
	template <DurationType D = milliseconds>
	std::size_t ConsumeAll(D compared_to) {
		if (compared_to <= D{ 0 }) {
			return 0;
		}

		const auto interval{ duration_cast<clock_duration>(compared_to) };
		if (interval <= clock_duration::zero()) {
			return 0;
		}

		const auto count{ static_cast<std::size_t>(elapsed_ / interval) };
		if (count == 0) {
			return 0;
		}

		elapsed_ -= interval * count;
		return count;
	}

	/// @return Elapsed fraction clamped to [0, 1]. Returns 1 for a zero duration.
	template <DurationType D = milliseconds>
	[[nodiscard]] float ElapsedFraction(D duration) const {
		if (duration == D{ 0 }) {
			return 1.0f;
		}

		using T = ptgn::duration<float, typename D::period>;
		T elapsed_time{ ElapsedDuration<T>() / duration };
		return Clamp01(elapsed_time.count());
	}

	bool operator==(const ManualTimer&) const = default;

	friend void to_json(json& j, const ManualTimer& timer) {
		j["running"] = timer.running_;
		j["paused"] = timer.paused_;
		j["has_run"] = timer.has_run_;
		j["elapsed"] = timer.ElapsedDuration<milliseconds>().count();
	}

	friend void from_json(const json& j, ManualTimer& timer) {
		timer.running_ = j.value("running", false);
		timer.paused_ = j.value("paused", false);
		timer.has_run_ = j.value("has_run", false);
		timer.elapsed_ = milliseconds{ j.value("elapsed", 0) };

		// Read the old offset field into elapsed time for compatibility with previously serialized data.
		if (const auto offset{ j.find("offset") }; offset != j.end() && offset->is_number_integer()) {
			timer.elapsed_ += milliseconds{ offset->get<milliseconds::rep>() };
			if (timer.elapsed_ < clock_duration::zero()) {
				timer.elapsed_ = clock_duration::zero();
			}
		}

		if (timer.paused_) {
			timer.running_ = false;
		}
	}

	friend std::ostream& operator<<(std::ostream& os, const ManualTimer& timer) {
		return os << "{ running: " << timer.running_ << ", paused: " << timer.paused_
				  << ", elapsed: " << timer.ElapsedDuration<secondsf>().count() << "s }";
	}

private:
	using clock_duration = std::chrono::steady_clock::duration;

	clock_duration elapsed_{};
	bool running_{ false };
	bool paused_{ false };
	bool has_run_{ false };

	PTGN_REFLECT(ManualTimer, elapsed_, running_, paused_, has_run_)
};

} // namespace ptgn