#include "utility/tween.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/game.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"
#include "utility/time.h"
#include "utility/utility.h"

namespace ptgn {

namespace impl {

template <typename T, typename... TArgs>
inline void InvokeCallback(const TweenCallback& callback, TArgs&&... args) {
	Invoke(std::get<T>(callback), std::forward<TArgs>(args)...);
}

TweenInstance::~TweenInstance() {}

bool TweenInstance::IsCompleted() const {
	return !tweens_points_.empty() && progress_ >= 1.0f &&
		   (index_ >= tweens_points_.size() - 1 || !started_);
}

float TweenInstance::GetNewProgress(duration<float> time) const {
	duration<float> progress{
		time / std::chrono::duration_cast<duration<float>>(GetCurrentTweenPoint().duration_)
	};
	float p{ progress.count() };
	if (std::isinf(p) || std::isnan(p)) {
		return 1.0f;
	}
	return progress_ + p;
}

float TweenInstance::GetProgress() const {
	auto& current{ GetCurrentTweenPoint() };

	float progress = current.currently_reversed_ ? 1.0f - progress_ : progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return std::invoke(current.easing_func_, progress, 0.0f, 1.0f);
}

const TweenPoint& TweenInstance::GetCurrentTweenPoint() const {
	PTGN_ASSERT(!tweens_points_.empty());
	PTGN_ASSERT(index_ <= tweens_points_.size());
	if (index_ == tweens_points_.size()) {
		return tweens_points_.back();
	}
	return tweens_points_[index_];
}

TweenPoint& TweenInstance::GetCurrentTweenPoint() {
	return const_cast<TweenPoint&>(const_cast<const TweenInstance&>(*this).GetCurrentTweenPoint());
}

TweenPoint& TweenInstance::GetLastTweenPoint() {
	PTGN_ASSERT(
		!tweens_points_.empty(), "TweenInstance must be given duration before setting properties"
	);
	return tweens_points_.back();
}

} // namespace impl

Tween& Tween::During(milliseconds duration) {
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");
	Create();
	Get().tweens_points_.emplace_back(duration);
	return *this;
}

float Tween::Step(float dt) {
	return StepImpl(dt, true);
}

float Tween::Seek(float new_progress) {
	return SeekImpl(AccumulateProgress(new_progress));
}

float Tween::Seek(milliseconds time) {
	return SeekImpl(AccumulateProgress(Get().GetNewProgress(time)));
}

float Tween::GetProgress() const {
	auto& t{ Get() };
	return t.GetProgress();
}

bool Tween::IsCompleted() const {
	return IsValid() && Get().IsCompleted();
}

bool Tween::IsRunning() const {
	if (!IsValid()) {
		return false;
	}
	auto& t{ Get() };
	return t.started_ && !t.paused_;
}

bool Tween::IsStarted() const {
	return IsValid() && Get().started_;
}

bool Tween::IsPaused() const {
	return IsValid() && Get().paused_;
}

std::int64_t Tween::GetRepeats() const {
	return Get().GetCurrentTweenPoint().current_repeat_;
}

Tween& Tween::Repeat(std::int64_t repeats) {
	PTGN_ASSERT(repeats == -1 || repeats > 0);
	auto& total_repeats{ Get().GetLastTweenPoint().total_repeats_ };
	total_repeats = repeats;
	if (total_repeats != -1) {
		// +1 because the first pass is not counted as a repeat.
		total_repeats += 1;
	}
	return *this;
}

Tween& Tween::Ease(TweenEase ease) {
	auto it = impl::tween_ease_functions_.find(ease);
	PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Could not identify tween easing type");
	Get().GetLastTweenPoint().easing_func_ = it->second;
	return *this;
}

Tween& Tween::Reverse(bool reversed) {
	if (IsStarted()) {
		Get().GetLastTweenPoint().currently_reversed_ = reversed;
	} else {
		Get().GetLastTweenPoint().SetReversed(reversed);
	}
	return *this;
}

Tween& Tween::Yoyo(bool yoyo) {
	Get().GetLastTweenPoint().yoyo_ = yoyo;
	return *this;
}

Tween& Tween::Clear() {
	Reset();
	Get().tweens_points_.clear();
	return *this;
}

Tween& Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");
	auto& t{ Get() };
	PTGN_ASSERT(
		tween_point_index < t.tweens_points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	t.tweens_points_[tween_point_index].duration_ = duration;
	UpdateImpl();
	PTGN_ASSERT(IsValid(), "Duration causes tween to be instantly completed and destroyed");
	return *this;
}

Tween& Tween::OnUpdate(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_update_ = callback;
	return *this;
}

Tween& Tween::OnStart(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_start_ = callback;
	return *this;
}

Tween& Tween::OnComplete(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_complete_ = callback;
	return *this;
}

Tween& Tween::OnStop(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_stop_ = callback;
	return *this;
}

Tween& Tween::OnPause(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_pause_ = callback;
	return *this;
}

Tween& Tween::OnResume(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_resume_ = callback;
	return *this;
}

Tween& Tween::OnRepeat(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_repeat_ = callback;
	return *this;
}

Tween& Tween::OnYoyo(const TweenCallback& callback) {
	Get().GetLastTweenPoint().on_yoyo_ = callback;
	return *this;
}

Tween& Tween::OnReset(const TweenCallback& callback) {
	Get().on_reset_ = callback;
	return *this;
}

void Tween::ActivateCallback(const TweenCallback& callback) {
	if (std::holds_alternative<std::function<void()>>(callback)) {
		impl::InvokeCallback<std::function<void()>>(callback);
	} else if (std::holds_alternative<std::function<void(float)>>(callback)) {
		impl::InvokeCallback<std::function<void(float)>>(callback, GetProgress());
	} else if (std::holds_alternative<std::function<void(Tween&)>>(callback)) {
		impl::InvokeCallback<std::function<void(Tween&)>>(callback, *this);
	} else if (std::holds_alternative<std::function<void(Tween&, float)>>(callback)) {
		impl::InvokeCallback<std::function<void(Tween&, float)>>(callback, *this, GetProgress());
	} else {
		PTGN_ERROR("Failed to identify tween callback function");
	}
}

void Tween::PointCompleted() {
	auto& t{ Get() };
	if (t.tweens_points_.empty()) {
		return;
	}
	ActivateCallback(t.GetCurrentTweenPoint().on_complete_);
	if (t.index_ < t.tweens_points_.size() - 1) {
		t.index_++;
		t.progress_									 = 0.0f;
		t.GetCurrentTweenPoint().currently_reversed_ = t.GetCurrentTweenPoint().start_reversed_;
		ActivateCallback(t.GetCurrentTweenPoint().on_start_);
	} else {
		t.progress_ = 1.0f;
		t.started_	= false;
	}
}

void Tween::HandleCallbacks(bool suppress_update) {
	if (!IsStarted() || IsPaused()) {
		return;
	}

	auto& t{ Get() };

	auto& current{ t.GetCurrentTweenPoint() };

	if (!suppress_update) {
		ActivateCallback(current.on_update_);
	}

	PTGN_ASSERT(t.progress_ <= 1.0f);

	// TweenInstance has not reached end of repetition.
	if (t.progress_ < 1.0f) {
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
	t.progress_ = 0.0f;
	ActivateCallback(current.on_repeat_);
}

float Tween::UpdateImpl(bool suppress_update) {
	auto& t{ Get() };

	PTGN_ASSERT(t.progress_ <= 1.0f);

	if (auto& current{ t.GetCurrentTweenPoint() };
		t.progress_ >= 1.0f &&
		(current.current_repeat_ < current.total_repeats_ || current.total_repeats_ == -1)) {
		current.current_repeat_++;
	}

	HandleCallbacks(suppress_update);

	// After completion and destruction.
	if (!IsValid() || (!t.started_ && t.progress_ == 1.0f)) {
		return 1.0f;
	}

	return t.GetProgress();
}

Tween& Tween::Pause() {
	if (auto& t{ Get() }; !t.paused_) {
		t.paused_ = true;
		if (!t.tweens_points_.empty()) {
			ActivateCallback(t.GetCurrentTweenPoint().on_pause_);
		}
	}
	return *this;
}

Tween& Tween::Resume() {
	if (auto& t{ Get() }; t.paused_) {
		t.paused_ = false;
		if (!t.tweens_points_.empty()) {
			ActivateCallback(t.GetCurrentTweenPoint().on_resume_);
		}
	}
	return *this;
}

Tween& Tween::Reset() {
	auto& t{ Get() };
	if (t.started_ || t.IsCompleted()) {
		ActivateCallback(t.on_reset_);
	}
	t.index_	= 0;
	t.progress_ = 0.0f;
	t.started_	= false;
	t.paused_	= false;
	for (auto& point : t.tweens_points_) {
		point.current_repeat_	  = 0;
		point.currently_reversed_ = point.start_reversed_;
	}
	return *this;
}

Tween& Tween::Start() {
	Reset();
	auto& t{ Get() };
	t.started_ = true;
	if (!t.tweens_points_.empty()) {
		ActivateCallback(t.GetCurrentTweenPoint().on_start_);
	}
	return *this;
}

Tween& Tween::IncrementTweenPoint() {
	if (IsCompleted()) {
		return *this;
	}
	// Cannot increment final tween point any further.
	if (const auto& t{ Get() }; t.index_ >= t.tweens_points_.size() - 1) {
		return *this;
	}
	PointCompleted();
	return *this;
}

Tween& Tween::StartIfNotRunning() {
	if (IsRunning()) {
		return *this;
	}
	Start();
	return *this;
}

Tween& Tween::Stop() {
	if (auto& t{ Get() }; t.started_) {
		if (!t.tweens_points_.empty()) {
			ActivateCallback(t.GetCurrentTweenPoint().on_stop_);
		}
		t.started_ = false;
	}
	return *this;
}

float Tween::StepImpl(float dt, bool accumulate_progress) {
	const auto& t{ Get() };
	return SeekImpl(
		accumulate_progress ? AccumulateProgress(t.GetNewProgress(duration<float>(dt)))
							: t.GetNewProgress(duration<float>(dt))
	);
}

float Tween::SeekImpl(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	// After completion and destruction.
	if (!IsValid()) {
		return 1.0f;
	}

	auto& t{ Get() };

	if (!t.started_ || t.paused_) {
		return t.GetProgress();
	}

	t.progress_ = new_progress;

	return UpdateImpl(false);
}

float Tween::AccumulateProgress(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f);
	PTGN_ASSERT(!std::isnan(new_progress));
	PTGN_ASSERT(!std::isinf(new_progress));

	if (new_progress < 1.0f) {
		return new_progress;
	}

	auto& t{ Get() };

	if (!t.started_ || t.paused_) {
		return GetProgress();
	}

	std::size_t loops{ static_cast<std::size_t>(std::floorf(new_progress)) };

	for (std::size_t i{ 0 }; i < loops; i++) {
		t.progress_ = 1.0f;
		UpdateImpl(true);
		if (!IsValid() || IsCompleted()) {
			return 1.0f;
		}
	}

	PTGN_ASSERT(new_progress >= loops);

	new_progress -= static_cast<float>(loops);

	return new_progress;
}

namespace impl {

void TweenManager::Update() {
	// TODO: Figure out how to do timestep accumulation outside of tweens, using
	// StepImpl(dt, false) and some added logic outside of this loop. This is important
	// because currently tween internal timestep accumulation causes all callbacks to be
	// triggered sequentially for each tween before moving onto the next tween. Desired
	// callback behavior:
	// 1. Tween1Repeat#1 2. Tween2Repeat#1 3. Tween1Repeat#2 4. Tween2Repeat#2.
	// Current callback behavior:
	// 1. Tween1Repeat#1 2. Tween1Repeat#2 3. Tween2Repeat#1 4. Tween2Repeat#2.

	float dt{ game.dt() };

	auto step = [dt](auto& tween) {
		if (tween.IsValid()) {
			tween.Step(dt);
		}
	};

	// Copying container to avoid iterator invalidation, e.g. in case a tween callback adds another
	// tween.
	auto nameless_tweens{ GetNamelessContainer() };

	for (auto& tween : nameless_tweens) {
		step(tween);
	}

	// Copying container to avoid iterator invalidation, e.g. in case a tween callback adds another
	// tween.
	auto named_tweens{ GetMap() };

	for (auto& [key, tween] : named_tweens) {
		step(tween);
	}

	// Must be cleared because these containers have incremented the tween reference counters.
	nameless_tweens = {};
	named_tweens	= {};

	auto& v{ GetNamelessContainer() };

	for (auto it{ v.begin() }; it != v.end();) {
		// Erase all tween which have been destroyed, or have completed (or are unstarted) and their
		// handles only have one strong reference, meaning they only exist as nameless tweens in the
		// tween manager.
		if (!it->IsValid() ||
			it->GetPtr().use_count() <= 1 && (it->IsCompleted() || !it->IsStarted())) {
			it = v.erase(it);
		} else {
			++it;
		}
	}
}

} // namespace impl

} // namespace ptgn