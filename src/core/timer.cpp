#include "core/timer.h"

#include <chrono>

#include "scene/scene.h"
#include "serialization/json.h"

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

namespace impl {

void ScriptTimers::Update(Scene& scene) {
	for (auto [entity, scripts, script_timer] : scene.EntitiesWith<Scripts, ScriptTimers>()) {
		for (auto timer_it{ script_timer.timers.begin() }; timer_it != script_timer.timers.end();) {
			auto& [key, timer_info] = *timer_it;

			if (!timer_info.timer.IsRunning()) {
				PTGN_ASSERT(
					timer_info.timer.HasRun(),
					"Script timer must be started upon addition of script to entity"
				);
				timer_it++;
				continue;
			}

			auto script_it{ scripts.scripts.find(key) };

			PTGN_ASSERT(
				script_it != scripts.scripts.end(),
				"Each script timer must have an associated script"
			);

			PTGN_ASSERT(script_it->second != nullptr, "Cannot invoke nullptr script");

			auto& script{ *script_it->second };

			auto elapsed_fraction{ timer_info.timer.ElapsedPercentage(timer_info.duration) };
			PTGN_ASSERT(elapsed_fraction >= 0.0f && elapsed_fraction <= 1.0f);

			script.OnTimerUpdate(elapsed_fraction);

			if (elapsed_fraction < 1.0f) {
				timer_it++;
			} else {
				bool remove{ script.OnTimerStop() };
				timer_info.timer.Stop();
				if (remove) {
					scripts.RemoveScript(key);
					timer_it = script_timer.timers.erase(timer_it);
				}
			}
		}
		if (script_timer.timers.empty()) {
			entity.Remove<ScriptTimers>();
		}
	}

	scene.Refresh();
}

void ScriptRepeats::Update(Scene& scene) {
	for (auto [entity, scripts, script_repeat] : scene.EntitiesWith<Scripts, ScriptRepeats>()) {
		for (auto repeat_it{ script_repeat.repeats.begin() };
			 repeat_it != script_repeat.repeats.end();) {
			auto& [key, repeat_info] = *repeat_it;

			PTGN_ASSERT(
				repeat_info.timer.IsRunning(),
				"Script repeat delay timer must be started upon addition of script to entity"
			);

			auto script_it{ scripts.scripts.find(key) };

			PTGN_ASSERT(
				script_it != scripts.scripts.end(),
				"Each repeating script info must have an associated script"
			);

			PTGN_ASSERT(script_it->second != nullptr, "Cannot invoke nullptr script");

			auto& script{ *script_it->second };

			auto elapsed_fraction{ repeat_info.timer.ElapsedPercentage(repeat_info.delay) };

			PTGN_ASSERT(elapsed_fraction >= 0.0f && elapsed_fraction <= 1.0f);

			if (elapsed_fraction < 1.0f) {
				// Delay has not passed yet, do nothing.
				repeat_it++;
				continue;
			}

			// Repeat delay has fully elapsed.

			script.OnRepeatUpdate(repeat_info.current_executions);
			repeat_info.current_executions++;

			bool infinite_execution{ repeat_info.max_executions == -1 };

			if (!infinite_execution &&
				repeat_info.current_executions >= repeat_info.max_executions) {
				script.OnRepeatStop();
				repeat_it = script_repeat.repeats.erase(repeat_it);
			} else {
				repeat_info.timer.Start(true);
			}
		}
		if (script_repeat.repeats.empty()) {
			entity.Remove<ScriptRepeats>();
		}
	}

	scene.Refresh();
}

} // namespace impl

} // namespace ptgn
