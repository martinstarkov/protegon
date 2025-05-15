#include "tweening/tween.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "common/function.h"
#include "core/time.h"
#include "debug/log.h"

namespace ptgn {

namespace impl {

TweenEaseFunction GetEaseFunction(TweenEase V) {
	if (V == TweenEase::Linear) {
		return [](float t, float a, float b) {
			float c{ b - a };
			return a + t * c;
		};
	} else if (V == TweenEase::InSine) {
		return [](float t, float a, float b) {
			float c{ b - a };
			return -c * std::cos(t * half_pi<float>) + b;
		};
	} else if (V == TweenEase::OutSine) {
		return [](float t, float a, float b) {
			float c{ b - a };
			return c * std::sin(t * half_pi<float>) + a;
		};
	} else if (V == TweenEase::InOutSine) {
		return [](float t, float a, float b) {
			float c{ b - a };
			return -c / float{ 2 } * (std::cos(pi<float> * t) - float{ 1 }) + a;
		};
	} else {
		PTGN_ERROR("Invalid or unimplemented tween ease function");
	}
	// TODO: Implement these.
	// TweenEase::InQuad
	// TweenEase::OutQuad
	// TweenEase::InOutQuad
	// TweenEase::InCubic
	// TweenEase::OutCubic
	// TweenEase::InOutCubic
	// TweenEase::InExponential
	// TweenEase::OutExponential
	// TweenEase::InOutExponential
	// TweenEase::InCircular
	// TweenEase::OutCircular
	// TweenEase::InOutCircular
}

TweenPoint::TweenPoint(milliseconds duration) : duration_{ duration } {}

void TweenPoint::SetReversed(bool reversed) {
	start_reversed_		= reversed;
	currently_reversed_ = start_reversed_;
}

// TODO: Get rid of these.
// void TweenPoint::Serialize(ptgn::StreamWriter* w, const TweenPoint& tween_point) {
//	w->Write(tween_point.current_repeat_);
//	w->Write(tween_point.total_repeats_);
//	w->Write(tween_point.yoyo_);
//	w->Write(tween_point.currently_reversed_);
//	w->Write(tween_point.start_reversed_);
//	w->Write(tween_point.duration_);
//	w->Write(tween_point.easing_func_);
//}
//
// void TweenPoint::Deserialize(ptgn::StreamReader* r, TweenPoint& tween_point) {
//	r->Read(tween_point.current_repeat_);
//	r->Read(tween_point.total_repeats_);
//	r->Read(tween_point.yoyo_);
//	r->Read(tween_point.currently_reversed_);
//	r->Read(tween_point.start_reversed_);
//	r->Read(tween_point.duration_);
//	r->Read(tween_point.easing_func_);
//}

} // namespace impl

template <typename T, typename... TArgs>
inline void InvokeCallback(const TweenCallback& callback, TArgs&&... args) {
	Invoke(std::get<T>(callback), std::forward<TArgs>(args)...);
}

bool Tween::IsCompleted() const {
	return !tween_points_.empty() && progress_ >= 1.0f &&
		   (index_ >= tween_points_.size() - 1 || !started_);
}

float Tween::GetNewProgress(duration<float> time) const {
	duration<float> progress{
		time / std::chrono::duration_cast<duration<float>>(GetCurrentTweenPoint().duration_)
	};
	float p{ progress.count() };
	if (std::isinf(p) || std::isnan(p)) {
		return 1.0f;
	}
	return progress_ + p;
}

float Tween::GetProgress() const {
	if (tween_points_.empty()) {
		return progress_;
	}

	auto& current{ GetCurrentTweenPoint() };

	float progress = current.currently_reversed_ ? 1.0f - progress_ : progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return std::invoke(impl::GetEaseFunction(current.easing_func_), progress, 0.0f, 1.0f);
}

const impl::TweenPoint& Tween::GetCurrentTweenPoint() const {
	PTGN_ASSERT(!tween_points_.empty());
	PTGN_ASSERT(index_ <= tween_points_.size());
	if (index_ == tween_points_.size()) {
		return tween_points_.back();
	}
	return tween_points_[index_];
}

impl::TweenPoint& Tween::GetCurrentTweenPoint() {
	return const_cast<impl::TweenPoint&>(std::as_const(*this).GetCurrentTweenPoint());
}

impl::TweenPoint& Tween::GetLastTweenPoint() {
	PTGN_ASSERT(!tween_points_.empty(), "Tween must be given duration before setting properties");
	return tween_points_.back();
}

Tween::Tween(Tween&& other) noexcept :
	progress_{ std::exchange(other.progress_, 0.0f) },
	index_{ std::exchange(other.index_, 0) },
	tween_points_{ std::exchange(other.tween_points_, {}) },
	paused_{ std::exchange(other.paused_, false) },
	started_{ std::exchange(other.started_, false) },
	on_reset_{ std::exchange(other.on_reset_, {}) } {}

Tween& Tween::operator=(Tween&& other) noexcept {
	if (this != &other) {
		progress_	  = std::exchange(other.progress_, 0.0f);
		index_		  = std::exchange(other.index_, 0);
		tween_points_ = std::exchange(other.tween_points_, {});
		on_reset_	  = std::exchange(other.on_reset_, {});
		paused_		  = std::exchange(other.paused_, false);
		started_	  = std::exchange(other.started_, false);
	}
	return *this;
}

Tween::~Tween() {
	progress_	  = 0.0f;
	index_		  = 0;
	tween_points_ = {};
	on_reset_	  = {};
	paused_		  = false;
	started_	  = false;
}

Tween& Tween::During(milliseconds duration) {
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");
	tween_points_.emplace_back(duration);
	return *this;
}

float Tween::Step(float dt) {
	return StepImpl(dt, true);
}

float Tween::Seek(float new_progress) {
	if (!started_ || paused_ || tween_points_.empty()) {
		return GetProgress();
	}
	return SeekImpl(AccumulateProgress(new_progress));
}

float Tween::Seek(milliseconds time) {
	if (!started_ || paused_ || tween_points_.empty()) {
		return GetProgress();
	}
	return SeekImpl(AccumulateProgress(GetNewProgress(time)));
}

bool Tween::IsRunning() const {
	return started_ && !paused_;
}

bool Tween::IsStarted() const {
	return started_;
}

bool Tween::IsPaused() const {
	return paused_;
}

std::int64_t Tween::GetRepeats() const {
	return GetCurrentTweenPoint().current_repeat_;
}

Tween& Tween::Repeat(std::int64_t repeats) {
	PTGN_ASSERT(repeats == -1 || repeats > 0);
	auto& total_repeats{ GetLastTweenPoint().total_repeats_ };
	total_repeats = repeats;
	if (total_repeats != -1) {
		// +1 because the first pass is not counted as a repeat.
		total_repeats += 1;
	}
	return *this;
}

Tween& Tween::Ease(TweenEase ease) {
	GetLastTweenPoint().easing_func_ = ease;
	return *this;
}

Tween& Tween::Reverse(bool reversed) {
	if (IsStarted()) {
		GetLastTweenPoint().currently_reversed_ = reversed;
	} else {
		GetLastTweenPoint().SetReversed(reversed);
	}
	return *this;
}

Tween& Tween::Yoyo(bool yoyo) {
	GetLastTweenPoint().yoyo_ = yoyo;
	return *this;
}

Tween& Tween::Clear() {
	Reset();
	tween_points_.clear();
	return *this;
}

milliseconds Tween::GetDuration(std::size_t tween_point_index) const {
	PTGN_ASSERT(
		tween_point_index < tween_points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	return tween_points_[tween_point_index].duration_;
}

Tween& Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");

	PTGN_ASSERT(
		tween_point_index < tween_points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	tween_points_[tween_point_index].duration_ = duration;
	UpdateImpl();
	return *this;
}

// TODO: Get rid of these.
/*
void Tween::Serialize(StreamWriter* w, const Tween& tween) {
	w->Write(tween.progress_);
	w->Write(tween.index_);
	w->Write(tween.tween_points_);
	w->Write(tween.paused_);
	w->Write(tween.started_);
}

void Tween::Deserialize(StreamReader* w, Tween& tween) {
	w->Read(tween.progress_);
	w->Read(tween.index_);
	w->Read(tween.tween_points_);
	w->Read(tween.paused_);
	w->Read(tween.started_);
}
*/

Tween& Tween::OnUpdate(const TweenCallback& callback) {
	GetLastTweenPoint().on_update_ = callback;
	return *this;
}

Tween& Tween::OnStart(const TweenCallback& callback) {
	GetLastTweenPoint().on_start_ = callback;
	return *this;
}

Tween& Tween::OnComplete(const TweenCallback& callback) {
	GetLastTweenPoint().on_complete_ = callback;
	return *this;
}

Tween& Tween::OnStop(const TweenCallback& callback) {
	GetLastTweenPoint().on_stop_ = callback;
	return *this;
}

Tween& Tween::OnPause(const TweenCallback& callback) {
	GetLastTweenPoint().on_pause_ = callback;
	return *this;
}

Tween& Tween::OnResume(const TweenCallback& callback) {
	GetLastTweenPoint().on_resume_ = callback;
	return *this;
}

Tween& Tween::OnRepeat(const TweenCallback& callback) {
	GetLastTweenPoint().on_repeat_ = callback;
	return *this;
}

Tween& Tween::OnYoyo(const TweenCallback& callback) {
	GetLastTweenPoint().on_yoyo_ = callback;
	return *this;
}

Tween& Tween::OnReset(const TweenCallback& callback) {
	on_reset_ = callback;
	return *this;
}

void Tween::ActivateCallback(const TweenCallback& callback) {
	if (std::holds_alternative<std::function<void()>>(callback)) {
		InvokeCallback<std::function<void()>>(callback);
	} else if (std::holds_alternative<std::function<void(float)>>(callback)) {
		InvokeCallback<std::function<void(float)>>(callback, GetProgress());
	} else if (std::holds_alternative<std::function<void(Tween&)>>(callback)) {
		InvokeCallback<std::function<void(Tween&)>>(callback, *this);
	} else if (std::holds_alternative<std::function<void(Tween&, float)>>(callback)) {
		InvokeCallback<std::function<void(Tween&, float)>>(callback, *this, GetProgress());
	} else {
		PTGN_ERROR("Failed to identify tween callback function");
	}
}

void Tween::PointCompleted() {
	if (tween_points_.empty()) {
		return;
	}
	ActivateCallback(GetCurrentTweenPoint().on_complete_);
	if (!tween_points_.empty() && index_ < tween_points_.size() - 1) {
		index_++;
		progress_								   = 0.0f;
		GetCurrentTweenPoint().currently_reversed_ = GetCurrentTweenPoint().start_reversed_;
		if (started_) {
			ActivateCallback(GetCurrentTweenPoint().on_start_);
		}
	} else {
		progress_ = 1.0f;
		started_  = false;
	}
}

void Tween::HandleCallbacks(bool suppress_update) {
	if (!IsStarted() || IsPaused()) {
		return;
	}

	auto& current{ GetCurrentTweenPoint() };

	if (!suppress_update) {
		ActivateCallback(current.on_update_);
	}

	PTGN_ASSERT(progress_ <= 1.0f);

	// Tween has not reached end of repetition.
	if (progress_ < 1.0f) {
		return;
	}

	// Completed tween.
	if (current.current_repeat_ == current.total_repeats_) {
		if (suppress_update) {
			ActivateCallback(current.on_update_);
		}
		PointCompleted();
		return;
	}

	// Reverse yoyoing tweens.
	if (current.yoyo_) {
		current.currently_reversed_ = !current.currently_reversed_;
		ActivateCallback(current.on_yoyo_);
	}

	// Repeat the tween.
	progress_ = 0.0f;
	ActivateCallback(current.on_repeat_);
}

float Tween::UpdateImpl(bool suppress_update) {
	PTGN_ASSERT(progress_ <= 1.0f);

	if (auto& current{ GetCurrentTweenPoint() };
		progress_ >= 1.0f &&
		(current.current_repeat_ < current.total_repeats_ || current.total_repeats_ == -1)) {
		current.current_repeat_++;
	}

	HandleCallbacks(suppress_update);

	// After completion and destruction.
	if (!started_ && progress_ == 1.0f) {
		return 1.0f;
	}

	return GetProgress();
}

Tween& Tween::Pause() {
	if (!paused_) {
		paused_ = true;
		if (!tween_points_.empty()) {
			ActivateCallback(GetCurrentTweenPoint().on_pause_);
		}
	}
	return *this;
}

Tween& Tween::Resume() {
	if (paused_) {
		paused_ = false;
		if (!tween_points_.empty()) {
			ActivateCallback(GetCurrentTweenPoint().on_resume_);
		}
	}
	return *this;
}

Tween& Tween::Reset() {
	if (started_ || IsCompleted()) {
		ActivateCallback(on_reset_);
	}
	index_	  = 0;
	progress_ = 0.0f;
	started_  = false;
	paused_	  = false;
	for (auto& point : tween_points_) {
		point.current_repeat_	  = 0;
		point.currently_reversed_ = point.start_reversed_;
	}
	return *this;
}

Tween& Tween::Start(bool force) {
	if (!force && IsRunning()) {
		return *this;
	}
	Reset();
	started_ = true;
	if (!tween_points_.empty()) {
		ActivateCallback(GetCurrentTweenPoint().on_start_);
	}
	return *this;
}

Tween& Tween::IncrementTweenPoint() {
	if (IsCompleted()) {
		return *this;
	}
	// Cannot increment tween point any further.
	if (index_ >= tween_points_.size()) {
		return *this;
	}
	PointCompleted();
	return *this;
}

Tween& Tween::Toggle() {
	if (IsStarted()) {
		Stop();
	} else {
		Start();
	}
	return *this;
}

Tween& Tween::Stop() {
	if (started_) {
		if (!tween_points_.empty()) {
			ActivateCallback(GetCurrentTweenPoint().on_stop_);
		}
		started_ = false;
	}
	return *this;
}

float Tween::StepImpl(float dt, bool accumulate_progress) {
	if (!started_ || paused_ || tween_points_.empty() || IsCompleted()) {
		return GetProgress();
	}
	float new_progress{ GetNewProgress(duration<float>(dt)) };
	return SeekImpl(accumulate_progress ? AccumulateProgress(new_progress) : new_progress);
}

float Tween::SeekImpl(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	if (!started_ || paused_ || tween_points_.empty()) {
		return GetProgress();
	}

	progress_ = new_progress;

	return UpdateImpl(false);
}

float Tween::AccumulateProgress(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f);
	PTGN_ASSERT(!std::isnan(new_progress));
	PTGN_ASSERT(!std::isinf(new_progress));

	if (new_progress < 1.0f) {
		return new_progress;
	}

	if (!started_ || paused_) {
		return GetProgress();
	}

	std::size_t loops{ static_cast<std::size_t>(std::floorf(new_progress)) };

	for (std::size_t i{ 0 }; i < loops; i++) {
		progress_ = 1.0f;
		UpdateImpl(true);
		if (IsCompleted()) {
			return 1.0f;
		}
	}

	PTGN_ASSERT(new_progress >= loops);

	new_progress -= static_cast<float>(loops);

	return new_progress;
}

} // namespace ptgn