#include "core/utils/timer.h"

#include <chrono>

#include "serialization/json/json.h"

namespace ptgn {

Timer::Timer(bool start) {
	if (start) {
		Start();
	}
}

void Timer::Reset() {
	start_time_ = std::chrono::steady_clock::now();
	pause_time_ = std::chrono::steady_clock::now();
	offset_		= std::chrono::steady_clock::duration::zero();
	Stop();
}

bool Timer::Start(bool force) {
	if (!force && IsRunning()) {
		return false;
	}
	start_time_ = std::chrono::steady_clock::now();
	running_	= true;
	paused_		= false;
	return true;
}

void Timer::Stop() {
	stop_time_ = std::chrono::steady_clock::now();
	running_   = false;
	paused_	   = false;
}

void Timer::Toggle() {
	if (IsRunning()) {
		Stop();
	} else {
		Start();
	}
}

void Timer::Pause() {
	if (running_ && !paused_) {
		stop_time_	= std::chrono::steady_clock::now();
		pause_time_ = std::chrono::steady_clock::now();
		running_	= false;
		paused_		= true;
	}
}

void Timer::Resume() {
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

bool Timer::IsPaused() const {
	return paused_;
}

bool Timer::IsRunning() const {
	return running_;
}

bool Timer::HasRun() const {
	return start_time_ != stop_time_;
}

void to_json(json& j, const Timer& timer) {
	j["running"] = timer.running_;
	j["paused"]	 = timer.paused_;
}

void from_json(const json& j, Timer& timer) {
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

} // namespace ptgn