#include "core/timer.h"

#include <chrono>

namespace ptgn {

Timer::Timer(bool start) {
	if (start) {
		Start();
	}
}

void Timer::Reset() {
	start_time_ = std::chrono::steady_clock::now();
	stop_time_	= std::chrono::steady_clock::now();
	pause_time_ = std::chrono::steady_clock::now();
	running_	= false;
	paused_		= false;
}

void Timer::Start(bool force) {
	if (!force && IsRunning()) {
		return;
	}
	start_time_ = std::chrono::steady_clock::now();
	running_	= true;
	paused_		= false;
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

} // namespace ptgn
