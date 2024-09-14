#include "protegon/tween.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "protegon/log.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/time.h"

namespace ptgn {

namespace impl {

template <typename T, typename... TArgs>
inline void InvokeCallback(const TweenCallback& callback, TArgs&&... args) {
	auto& f = std::get<T>(callback);
	if (f) {
		std::invoke(f, std::forward<TArgs>(args)...);
	}
}

} // namespace impl

Tween::Tween(milliseconds duration) {
	During(duration);
}

Tween& Tween::During(milliseconds duration) {
	PTGN_ASSERT(duration > nanoseconds{ 250 });
	Create();
	Get().tweens_points_.emplace_back(duration);
	return *this;
}

Tween& Tween::Start() {
	Reset();
	Get().started_ = true;
	ActivateCallback(GetCurrentTweenPoint().on_start_);
	return *this;
}

Tween& Tween::Reset() {
	if (IsStarted() || IsCompleted()) {
		ActivateCallback(GetCurrentTweenPoint().on_stop_);
	}
	auto& t{ Get() };
	t.index_	= 0;
	t.progress_ = 0.0f;
	t.started_	= false;
	t.paused_	= false;
	for (auto& point : t.tweens_points_) {
		point.current_repeat_ = 0;
	}
	return *this;
}

Tween& Tween::Pause() {
	if (auto& t{ Get() }; !t.paused_) {
		t.paused_ = true;
		ActivateCallback(GetCurrentTweenPoint().on_pause_);
	}
	return *this;
}

Tween& Tween::Resume() {
	if (auto& t{ Get() }; t.paused_) {
		t.paused_ = false;
		ActivateCallback(GetCurrentTweenPoint().on_resume_);
	}
	return *this;
}

/*float Tween::Rewind(float dt) {
	return Step(-dt);
}*/

float Tween::GetNewProgress(duration<float> time) const {
	duration<float> progress{
		time / std::chrono::duration_cast<duration<float>>(GetCurrentTweenPoint().duration_)
	};
	float p{ progress.count() };
	if (std::isinf(p) || std::isnan(p)) {
		return 1.0f;
	}
	auto& t{ Get() };
	return t.progress_ + p;
}

float Tween::Step(float dt) {
	return StepImpl(dt, true);
}

float Tween::StepImpl(float dt, bool accumulate_progress) {
	return SeekImpl(
		accumulate_progress ? AccumulateProgress(GetNewProgress(duration<float>(dt)))
							: GetNewProgress(duration<float>(dt))
	);
}

float Tween::Seek(float new_progress) {
	return SeekImpl(AccumulateProgress(new_progress));
}

float Tween::Seek(milliseconds time) {
	return SeekImpl(AccumulateProgress(GetNewProgress(time)));
}

float Tween::SeekImpl(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	auto& t{ Get() };
	if (!t.started_ || t.paused_) {
		return GetProgress();
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
		if (IsCompleted()) {
			return 1.0f;
		}
	}

	PTGN_ASSERT(new_progress >= loops);

	new_progress -= static_cast<float>(loops);

	return new_progress;
}

float Tween::GetProgress() const {
	auto& current{ GetCurrentTweenPoint() };

	auto& t{ Get() };

	float progress = current.reversed_ ? 1.0f - t.progress_ : t.progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return std::invoke(current.easing_func_, progress, 0.0f, 1.0f);
}

bool Tween::IsCompleted() const {
	auto& t{ Get() };
	return !t.tweens_points_.empty() && t.progress_ >= 1.0f &&
		   (t.index_ >= t.tweens_points_.size() - 1 || !t.started_);
}

bool Tween::IsStarted() const {
	return IsValid() && Get().started_;
}

bool Tween::IsPaused() const {
	return IsValid() && Get().paused_;
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
	auto it = impl::tween_ease_functions_.find(ease);
	PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Could not identify tween easing type");
	GetLastTweenPoint().easing_func_ = it->second;
	return *this;
}

Tween& Tween::Reverse(bool reversed) {
	GetLastTweenPoint().reversed_ = reversed;
	return *this;
}

Tween& Tween::Yoyo(bool yoyo) {
	GetLastTweenPoint().yoyo_ = yoyo;
	return *this;
}

Tween& Tween::Forward() {
	Reverse(false);
	return *this;
}

Tween& Tween::Backward() {
	Reverse(true);
	return *this;
}

Tween& Tween::Clear() {
	Reset();
	Get().tweens_points_.clear();
	return *this;
}

Tween& Tween::Complete() {
	Seek(GetCurrentTweenPoint().reversed_ ? 0.0f : 1.0f);
	return *this;
}

Tween& Tween::Stop() {
	if (auto& t{ Get() }; t.started_) {
		ActivateCallback(GetCurrentTweenPoint().on_stop_);
		t.started_ = false;
	}
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

Tween& Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(duration > nanoseconds{ 250 });
	auto& t{ Get() };
	PTGN_ASSERT(
		tween_point_index < t.tweens_points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	t.tweens_points_[tween_point_index].duration_ = duration;
	UpdateImpl();
	return *this;
}

void Tween::PointCompleted() {
	ActivateCallback(GetCurrentTweenPoint().on_complete_);
	auto& t{ Get() };
	if (t.index_ < t.tweens_points_.size() - 1) {
		t.index_++;
		t.progress_ = 0.0f;
		ActivateCallback(GetCurrentTweenPoint().on_start_);
	} else {
		t.progress_ = 1.0f;
		t.started_	= false;
	}
}

void Tween::HandleCallbacks(bool suppress_update) {
	auto& t{ Get() };
	if (!t.started_ || t.paused_) {
		return;
	}

	auto& current{ GetCurrentTweenPoint() };

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
		current.reversed_ = !current.reversed_;
		ActivateCallback(current.on_yoyo_);
	}

	// Repeat the tween.
	t.progress_ = 0.0f;
	ActivateCallback(current.on_repeat_);
}

float Tween::UpdateImpl(bool suppress_update) {
	const auto& t{ Get() };
	PTGN_ASSERT(t.progress_ <= 1.0f);

	if (auto& current{ GetCurrentTweenPoint() };
		t.progress_ >= 1.0f &&
		(current.current_repeat_ < current.total_repeats_ || current.total_repeats_ == -1)) {
		current.current_repeat_++;
	}

	HandleCallbacks(suppress_update);

	return GetProgress();
}

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

const impl::TweenPoint& Tween::GetCurrentTweenPoint() const {
	auto& t{ Get() };
	PTGN_ASSERT(t.tweens_points_.size() > 0);
	PTGN_ASSERT(t.index_ <= t.tweens_points_.size());
	if (t.index_ == t.tweens_points_.size()) {
		return t.tweens_points_.back();
	}
	return t.tweens_points_[t.index_];
}

impl::TweenPoint& Tween::GetCurrentTweenPoint() {
	return const_cast<impl::TweenPoint&>(const_cast<const Tween&>(*this).GetCurrentTweenPoint());
}

impl::TweenPoint& Tween::GetLastTweenPoint() {
	auto& t{ Get() };
	PTGN_ASSERT(
		t.tweens_points_.size() > 0,
		"TweenInstance must be given duration before setting properties"
	);
	return t.tweens_points_.back();
}

} // namespace ptgn
