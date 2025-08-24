#include "tweens/tween.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/time.h"
#include "math/easing.h"
#include "math/math.h"

#define PTGN_ADD_TWEEN_ACTION(FUNC_NAME) \
	GetCurrentTweenPoint().script_container_.AddAction(&TweenScript::FUNC_NAME)

#define PTGN_ADD_TWEEN_GLOBAL_ACTION(FUNC_NAME)                     \
	auto& FUNC_NAME##_tween{ Get<impl::TweenInstance>() };          \
	for (auto& point : tween.points_) {                             \
		point.script_container_.AddAction(&TweenScript::FUNC_NAME); \
	}

#define PTGN_ADD_TWEEN_PROGRESS_ACTION() \
	GetCurrentTweenPoint().script_container_.AddAction(&TweenScript::OnProgress, GetProgress())

namespace ptgn {

Tween::Tween(const Entity& entity) : Entity{ entity } {}

Tween& Tween::During(milliseconds duration) {
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");
	auto& tween{ Get<impl::TweenInstance>() };
	tween.points_.emplace_back().duration_ = duration;
	return *this;
}

Tween& Tween::OnProgress(const std::function<void(Entity, float)>& func) {
	return AddScript<impl::TweenProgressScript>(func);
}

Tween& Tween::OnStart(const TweenCallback& func) {
	return AddScript<impl::TweenStartScript>(func);
}

Tween& Tween::OnComplete(const TweenCallback& func) {
	return AddScript<impl::TweenCompleteScript>(func);
}

Tween& Tween::OnPointStart(const TweenCallback& func) {
	return AddScript<impl::TweenPointStartScript>(func);
}

Tween& Tween::OnPointComplete(const TweenCallback& func) {
	return AddScript<impl::TweenPointCompleteScript>(func);
}

Tween& Tween::OnReset(const TweenCallback& func) {
	return AddScript<impl::TweenResetScript>(func);
}

Tween& Tween::OnStop(const TweenCallback& func) {
	return AddScript<impl::TweenStopScript>(func);
}

Tween& Tween::OnPause(const TweenCallback& func) {
	return AddScript<impl::TweenPauseScript>(func);
}

Tween& Tween::OnResume(const TweenCallback& func) {
	return AddScript<impl::TweenResumeScript>(func);
}

Tween& Tween::OnYoyo(const TweenCallback& func) {
	return AddScript<impl::TweenYoyoScript>(func);
}

Tween& Tween::OnRepeat(const TweenCallback& func) {
	return AddScript<impl::TweenRepeatScript>(func);
}

bool Tween::IsCompleted() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.state_ == impl::TweenState::Completed;
}

bool Tween::IsRunning() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.state_ == impl::TweenState::Started;
}

bool Tween::IsStarted() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.state_ == impl::TweenState::Started || tween.state_ == impl::TweenState::Paused;
}

bool Tween::IsPaused() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.state_ == impl::TweenState::Paused;
}

Tween& Tween::Start(bool force) {
	if (!force && IsRunning()) {
		return *this;
	}
	Reset();
	auto& tween{ Get<impl::TweenInstance>() };
	tween.state_ = impl::TweenState::Started;
	PTGN_ADD_TWEEN_GLOBAL_ACTION(OnStart);
	PTGN_ADD_TWEEN_ACTION(OnPointStart);
	return *this;
}

Tween& Tween::Stop() {
	if (IsStarted() || IsPaused()) {
		auto& tween{ Get<impl::TweenInstance>() };
		tween.state_ = impl::TweenState::Stopped;
		PTGN_ADD_TWEEN_GLOBAL_ACTION(OnStop);
	}
	return *this;
}

Tween& Tween::Pause() {
	if (!IsRunning()) {
		return *this;
	}
	auto& tween{ Get<impl::TweenInstance>() };
	tween.state_ = impl::TweenState::Paused;
	PTGN_ADD_TWEEN_GLOBAL_ACTION(OnPause);
	return *this;
}

Tween& Tween::Resume() {
	if (!IsPaused()) {
		return *this;
	}
	auto& tween{ Get<impl::TweenInstance>() };
	tween.state_ = impl::TweenState::Started;
	PTGN_ADD_TWEEN_GLOBAL_ACTION(OnResume);
	return *this;
}

Tween& Tween::Toggle() {
	if (IsRunning()) {
		Pause();
	} else if (IsPaused()) {
		Resume();
	} else {
		Start();
	}
	return *this;
}

Tween& Tween::Reset() {
	bool was_started_or_completed{ IsStarted() || IsCompleted() };
	auto& tween{ Get<impl::TweenInstance>() };
	tween.progress_ = 0.0f;
	tween.index_	= 0;
	tween.state_	= impl::TweenState::Stopped;
	for (auto& point : tween.points_) {
		point.current_repeat_	  = 0;
		point.currently_reversed_ = point.start_reversed_;
	}
	if (was_started_or_completed) {
		for (auto& tween_point : tween.points_) {
			tween_point.script_container_.AddAction(&TweenScript::OnReset);
		}
	}
	return *this;
}

Tween& Tween::Clear() {
	auto& tween{ Get<impl::TweenInstance>() };
	tween.points_.clear();
	Reset();
	return *this;
}

Tween& Tween::Ease(const ptgn::Ease& ease) {
	GetLastTweenPoint().ease_ = ease;
	return *this;
}

Tween& Tween::Repeat(std::int64_t repeats) {
	if (repeats == 0) {
		return *this;
	}
	PTGN_ASSERT(
		repeats == -1 || repeats > 0, "Repeats cannot be negative unless it is -1 (infinite)"
	);
	auto& total_repeats{ GetLastTweenPoint().total_repeats_ };
	total_repeats = repeats;
	if (total_repeats != -1) {
		// +1 because the first pass is not counted as a repeat.
		total_repeats += 1;
	}
	return *this;
}

Tween& Tween::Reverse(bool reversed) {
	auto& tween_points{ GetLastTweenPoint() };
	tween_points.start_reversed_	 = reversed;
	tween_points.currently_reversed_ = reversed;
	return *this;
}

Tween& Tween::Yoyo(bool yoyo) {
	GetLastTweenPoint().yoyo_ = yoyo;
	return *this;
}

float Tween::GetLinearProgress() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	if (const auto& point{ GetCurrentTweenPoint() }; point.currently_reversed_) {
		return 1.0f - tween.progress_;
	}
	return tween.progress_;
}

float Tween::GetProgress() const {
	const auto& point{ GetCurrentTweenPoint() };
	return ApplyEase(GetLinearProgress(), point.ease_);
}

ptgn::Ease Tween::GetEase() const {
	const auto& point{ GetCurrentTweenPoint() };
	return point.ease_;
}

std::int64_t Tween::GetRepeats() const {
	if (const auto& tween{ Get<impl::TweenInstance>() }; tween.index_ < tween.points_.size()) {
		return tween.points_[tween.index_].current_repeat_;
	}
	return 0;
}

std::size_t Tween::GetCurrentIndex() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.index_;
}

Tween& Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(
		tween_point_index < tween.points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"before setting duration"
	);
	tween.points_[tween_point_index].duration_ = duration;
	return *this;
}

milliseconds Tween::GetDuration(std::size_t tween_point_index) const {
	const auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(
		tween_point_index < tween.points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added before "
		"getting duration"
	);
	return tween.points_[tween_point_index].duration_;
}

void Tween::Step(float dt) {
	auto& tween{ Get<impl::TweenInstance>() };

	if (dt <= 0.0f || tween.state_ != impl::TweenState::Started) {
		return;
	}

	if (tween.points_.empty()) {
		tween.state_ = impl::TweenState::Completed;
		PTGN_ADD_TWEEN_GLOBAL_ACTION(OnComplete);
		return;
	}

	while (dt > 0.0f && tween.state_ == impl::TweenState::Started) {
		impl::TweenPoint& point{ GetCurrentTweenPoint() };

		if (float duration_sec{ to_duration_value<secondsf>(point.duration_) };
			duration_sec <= 0.0f) {
			tween.progress_ = 1.0f;
			dt				= 0.0f;
		} else {
			float progress_inc{ dt / duration_sec };
			float new_progress{ tween.progress_ + progress_inc };

			if (new_progress >= 1.0f) {
				dt				= (new_progress - 1.0f) * duration_sec;
				tween.progress_ = 1.0f;
			} else {
				tween.progress_ = new_progress;
				dt				= 0.0f;
			}
		}

		PTGN_ADD_TWEEN_PROGRESS_ACTION();

		if (tween.progress_ >= 1.0f) {
			if (tween.points_.empty()) {
				continue;
			}

			point.current_repeat_++;

			bool infinite_repeat = point.total_repeats_ == -1;
			bool should_repeat	 = infinite_repeat || point.current_repeat_ < point.total_repeats_;

			if (point.yoyo_ && should_repeat) {
				point.currently_reversed_ = !point.currently_reversed_;
				tween.progress_			  = 0.0f;
				PTGN_ADD_TWEEN_ACTION(OnYoyo);
				continue;
			}

			if (should_repeat) {
				tween.progress_ = 0.0f;
				PTGN_ADD_TWEEN_ACTION(OnRepeat);
				continue;
			}

			IncrementPoint();
		}
	}
}

Tween& Tween::IncrementPoint() {
	auto& tween{ Get<impl::TweenInstance>() };
	if (tween.points_.empty()) {
		return *this;
	}
	// Move to next tween point.
	if (tween.index_ + 1 < tween.points_.size()) {
		PTGN_ADD_TWEEN_ACTION(OnPointComplete);
		tween.index_++;
		PTGN_ADD_TWEEN_ACTION(OnPointStart);
		tween.progress_ = 0.0f;

		// Reset repeat count and reversal
		impl::TweenPoint& new_point	  = GetCurrentTweenPoint();
		new_point.current_repeat_	  = 0;
		new_point.currently_reversed_ = new_point.start_reversed_;
	} else {
		if (tween.state_ != impl::TweenState::Completed) {
			PTGN_ADD_TWEEN_ACTION(OnPointComplete);
		}
		// No more points: complete
		tween.state_	= impl::TweenState::Completed;
		tween.progress_ = 1.0f;
		PTGN_ADD_TWEEN_GLOBAL_ACTION(OnComplete);
	}
	return *this;
}

Tween& Tween::RemoveLastTweenPoint() {
	auto& tween{ Get<impl::TweenInstance>() };
	if (tween.points_.empty()) {
		return *this;
	}

	auto last_index{ tween.points_.size() - 1 };
	tween.points_.pop_back();

	if (tween.index_ != 0 && tween.index_ >= last_index) {
		tween.index_ -= 1;
	}
	return *this;
}

void Tween::Seek(float new_progress) {
	new_progress = std::clamp(new_progress, 0.0f, 1.0f);

	Reset(); // Reset and re-simulate from 0.
	Start();

	float current_progress{ 0.0f };
	float target_progress{ new_progress };

	constexpr float step_size{ 1.0f / 120.0f }; // simulate ~120 fps (or use config value)

	const auto& tween{ Get<impl::TweenInstance>() };

	while (current_progress < target_progress && !IsCompleted()) {
		float before{ tween.progress_ };

		Step(step_size);

		current_progress = tween.progress_;

		if (std::abs(current_progress - before) <
			epsilon<float>) { // Avoid infinite loop on broken tweens
			break;
		}
	}
}

void Tween::Seek(milliseconds time) {
	float total_ms{ to_duration_value<millisecondsf>(GetTotalDuration()) };
	float target_progress{ to_duration_value<millisecondsf>(time) / total_ms };
	Seek(target_progress);

	// Alternative implementation.
	// float target_time{ to_duration_value<secondsf>(time) };
	//// Reset and simulate forward.
	// Reset();
	// Start();
	// float elapsed{ 0.0f };
	// constexpr float step_size{ 1.0f / 120.0f }; // simulate ~120 fps (or use config value)
	// while (elapsed < target_time && !IsCompleted()) {
	//	Step(step_size);
	//	elapsed += step_size;
	// }
}

std::size_t Tween::GetTweenPointCount() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.points_.size();
}

const impl::TweenPoint& Tween::GetTweenPoint(std::size_t tween_point_index) const {
	const auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(!tween.points_.empty(), "Cannot retrieve tween point when none have been added");
	PTGN_ASSERT(
		tween_point_index <= tween.points_.size(), "Tween point index out of range of tween points"
	);
	if (tween_point_index == tween.points_.size()) {
		return tween.points_.back();
	}
	return tween.points_[tween_point_index];
}

impl::TweenPoint& Tween::GetTweenPoint(std::size_t tween_point_index) {
	return const_cast<impl::TweenPoint&>(std::as_const(*this).GetTweenPoint(tween_point_index));
}

milliseconds Tween::GetTotalDuration() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	milliseconds total{ 0 };
	for (const auto& point : tween.points_) {
		total += point.duration_;
	}
	return total;
}

const impl::TweenPoint& Tween::GetCurrentTweenPoint() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return GetTweenPoint(tween.index_);
}

impl::TweenPoint& Tween::GetCurrentTweenPoint() {
	return const_cast<impl::TweenPoint&>(std::as_const(*this).GetCurrentTweenPoint());
}

impl::TweenPoint& Tween::GetLastTweenPoint() {
	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(!tween.points_.empty(), "Cannot get tween point when none have been added");
	return tween.points_.back();
}

const impl::TweenPoint& Tween::GetLastTweenPoint() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(!tween.points_.empty(), "Cannot get tween point when none have been added");
	return tween.points_.back();
}

void Tween::Update(Manager& manager, float dt) {
	const auto invoke_tween_scripts = [&]() {
		for (auto [entity, tween] : manager.EntitiesWith<impl::TweenInstance>()) {
			for (auto& point : tween.points_) {
				point.script_container_.InvokeActions();
			}
		}

		manager.Refresh();
	};

	invoke_tween_scripts();

	for (auto [entity, tween] : manager.EntitiesWith<impl::TweenInstance>()) {
		Tween{ entity }.Step(dt);
	}

	invoke_tween_scripts();
}

Tween CreateTween(Manager& manager) {
	Tween tween{ manager.CreateEntity() };

	tween.Add<impl::TweenInstance>();

	return tween;
}

} // namespace ptgn