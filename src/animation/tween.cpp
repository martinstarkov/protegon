#include "protegon/tween.h"

namespace ptgn {

namespace impl {

TweenInstance::~TweenInstance() {
	Tween tween;
	if (started_) {
		tween.instance_ = std::make_shared<TweenInstance>(*this);
		tween.Stop();
		// TODO: Consider destroying tween instance.
		// Destroy();
	}
}

const TweenPoint& TweenInstance::GetCurrentTweenPoint() const {
	PTGN_ASSERT(tweens_points_.size() > 0);
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
		tweens_points_.size() > 0, "Tween must be given duration before setting properties"
	);
	return tweens_points_.back();
}

} // namespace impl

Tween::Tween(milliseconds duration) {
	During(duration);
}

Tween& Tween::During(milliseconds duration) {
	PTGN_ASSERT(duration > nanoseconds{ 250 });
	if (!Handle::IsValid()) {
		instance_ = std::make_shared<impl::TweenInstance>();
	}
	instance_->tweens_points_.emplace_back(duration);
	return *this;
}

Tween& Tween::Start() {
	PTGN_ASSERT(IsValid(), "Cannot start uninitialized or destroyed tween");
	Reset();
	instance_->started_ = true;
	ActivateCallback(instance_->GetCurrentTweenPoint().on_start_, GetProgress());
	return *this;
}

void Tween::Reset() {
	PTGN_ASSERT(IsValid(), "Cannot reset uninitialized or destroyed tween");
	instance_->index_	 = 0;
	instance_->progress_ = 0.0f;
	instance_->started_	 = false;
	instance_->paused_	 = false;
	for (auto& point : instance_->tweens_points_) {
		point.current_repeat_ = 0;
	}
}

Tween& Tween::Pause() {
	PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
	if (!instance_->paused_) {
		instance_->paused_ = true;
		ActivateCallback(instance_->GetCurrentTweenPoint().on_pause_, GetProgress());
	}
	return *this;
}

void Tween::Resume() {
	PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
	if (instance_->paused_) {
		instance_->paused_ = false;
		ActivateCallback(instance_->GetCurrentTweenPoint().on_resume_, GetProgress());
	}
}

/*float Tween::Rewind(float dt) {
	return Step(-dt);
}*/

float Tween::GetNewProgress(duration<float> time) const {
	PTGN_ASSERT(IsValid(), "Cannot get new progress for uninitialized or destroyed tween");
	duration<float> progress{ time / std::chrono::duration_cast<duration<float>>(
										 instance_->GetCurrentTweenPoint().duration_
									 ) };
	float p{ progress.count() };
	if (std::isinf(p) || std::isnan(p)) {
		return 1.0f;
	}
	return instance_->progress_ + p;
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
	PTGN_ASSERT(IsValid(), "Cannot seek uninitialized or destroyed tween");

	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	if (!instance_->started_ || instance_->paused_) {
		return GetProgress();
	}

	instance_->progress_ = new_progress;

	return UpdateImpl(false);
}

float Tween::AccumulateProgress(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f);
	PTGN_ASSERT(!std::isnan(new_progress));
	PTGN_ASSERT(!std::isinf(new_progress));

	if (new_progress < 1.0f) {
		return new_progress;
	}

	if (!instance_->started_ || instance_->paused_) {
		return GetProgress();
	}

	float loops{ std::floorf(new_progress) };

	for (float i = 0; i < loops; i++) {
		instance_->progress_ = 1.0f;
		UpdateImpl(true);
		if (IsCompleted()) {
			return 1.0f;
		}
	}

	PTGN_ASSERT(new_progress >= loops);

	new_progress -= loops;

	return new_progress;
}

float Tween::GetProgress() const {
	PTGN_ASSERT(IsValid(), "Cannot get progress of uninitialized or destroyed tween");

	auto& current{ instance_->GetCurrentTweenPoint() };

	float progress = current.reversed_ ? 1.0f - instance_->progress_ : instance_->progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return std::invoke(current.easing_func_, progress, 0.0f, 1.0f);
}

bool Tween::IsCompleted() const {
	return IsValid() && instance_->progress_ >= 1.0f &&
		   (instance_->index_ >= instance_->tweens_points_.size() - 1 || !instance_->started_);
}

bool Tween::IsStarted() const {
	return IsValid() && instance_->started_;
}

bool Tween::IsPaused() const {
	return IsValid() && instance_->paused_;
}

bool Tween::IsValid() const {
	return Handle::IsValid() && instance_->tweens_points_.size() > 0;
}

std::int64_t Tween::GetRepeats() const {
	PTGN_ASSERT(IsValid(), "Cannot get repeats of uninitialized or destroyed tween");
	return instance_->GetCurrentTweenPoint().current_repeat_;
}

Tween& Tween::Repeat(std::int64_t repeats) {
	PTGN_ASSERT(IsValid(), "Cannot set repeats of uninitialized or destroyed tween");
	PTGN_ASSERT(repeats == -1 || repeats > 0);
	auto& total_repeats{ instance_->GetLastTweenPoint().total_repeats_ };
	total_repeats = repeats;
	if (total_repeats != -1) {
		// +1 because the first pass is not counted as a repeat.
		total_repeats += 1;
	}
	return *this;
}

Tween& Tween::Ease(TweenEase ease) {
	PTGN_ASSERT(IsValid(), "Cannot set easing function of uninitialized or destroyed tween");
	auto it = impl::tween_ease_functions_.find(ease);
	PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Could not identify tween easing type");
	instance_->GetLastTweenPoint().easing_func_ = it->second;
	return *this;
}

Tween& Tween::Reverse(bool reversed) {
	PTGN_ASSERT(IsValid(), "Cannot reverse uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().reversed_ = reversed;
	return *this;
}

Tween& Tween::Yoyo(bool yoyo) {
	PTGN_ASSERT(IsValid(), "Cannot yoyo uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().yoyo_ = yoyo;
	return *this;
}

void Tween::Forward() {
	Reverse(false);
}

void Tween::Backward() {
	Reverse(true);
}

void Tween::Clear() {
	PTGN_ASSERT(IsValid(), "Cannot clear uninitialized or destroyed tween");
	Reset();
	instance_->tweens_points_.clear();
}

void Tween::Complete() {
	Seek(instance_->GetCurrentTweenPoint().reversed_ ? 0.0f : 1.0f);
}

void Tween::Stop() {
	PTGN_ASSERT(IsValid(), "Cannot stop uninitialized or destroyed tween");
	if (instance_->started_) {
		ActivateCallback(instance_->GetCurrentTweenPoint().on_stop_, GetProgress());
		instance_->started_ = false;
		// TODO: Consider destroying tween instance.
		// Destroy();
	}
}

void Tween::Destroy() {
	instance_.reset();
}

template <typename T, typename... TArgs>
inline void InvokeCallback(const TweenCallback& callback, TArgs&&... args) {
	auto& f = std::get<T>(callback);
	if (f) {
		std::invoke(f, std::forward<TArgs>(args)...);
	}
}

void Tween::ActivateCallback(const TweenCallback& callback, float value) {
	if (std::holds_alternative<std::function<void()>>(callback)) {
		InvokeCallback<std::function<void()>>(callback);
	} else if (std::holds_alternative<std::function<void(float)>>(callback)) {
		InvokeCallback<std::function<void(float)>>(callback, value);
	} else if (std::holds_alternative<std::function<void(Tween&)>>(callback)) {
		InvokeCallback<std::function<void(Tween&)>>(callback, *this);
	} else if (std::holds_alternative<std::function<void(Tween&, float)>>(callback)) {
		InvokeCallback<std::function<void(Tween&, float)>>(callback, *this, value);
	} else {
		PTGN_ERROR("Failed to identify tween callback function");
	}
}

void Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(duration > nanoseconds{ 250 });
	PTGN_ASSERT(IsValid(), "Cannot set duration of uninitialized or destroyed tween");
	PTGN_ASSERT(
		tween_point_index < instance_->tweens_points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	instance_->tweens_points_[tween_point_index].duration_ = duration;
	UpdateImpl();
}

void Tween::PointCompleted() {
	ActivateCallback(instance_->GetCurrentTweenPoint().on_complete_, GetProgress());
	if (instance_->index_ < instance_->tweens_points_.size() - 1) {
		instance_->index_++;
		instance_->progress_ = 0.0f;
		ActivateCallback(instance_->GetCurrentTweenPoint().on_start_, GetProgress());
	} else {
		instance_->progress_ = 1.0f;
		instance_->started_	 = false;
	}
}

void Tween::HandleCallbacks(bool suppress_update) {
	if (!instance_->started_ || instance_->paused_) {
		return;
	}

	auto& current{ instance_->GetCurrentTweenPoint() };

	if (!suppress_update) {
		ActivateCallback(current.on_update_, GetProgress());
	}

	PTGN_ASSERT(instance_->progress_ <= 1.0f);

	// Tween has not reached end of repetition.
	if (instance_->progress_ < 1.0f) {
		return;
	}

	// Completed tween.
	if (current.current_repeat_ == current.total_repeats_) {
		if (suppress_update) {
			ActivateCallback(current.on_update_, GetProgress());
		}
		PointCompleted();
		return;
	}

	// Reverse yoyoing tweens.
	if (current.yoyo_) {
		current.reversed_ = !current.reversed_;
		ActivateCallback(current.on_yoyo_, GetProgress());
	}

	// Repeat the tween.
	instance_->progress_ = 0.0f;
	ActivateCallback(current.on_repeat_, GetProgress());
}

float Tween::UpdateImpl(bool suppress_update) {
	PTGN_ASSERT(IsValid(), "Cannot update uninitialized or destroyed tween");

	PTGN_ASSERT(instance_->progress_ <= 1.0f);

	auto& current{ instance_->GetCurrentTweenPoint() };

	if (instance_->progress_ >= 1.0f &&
		(current.current_repeat_ < current.total_repeats_ || current.total_repeats_ == -1)) {
		current.current_repeat_++;
	}

	HandleCallbacks(suppress_update);

	return GetProgress();
}

Tween& Tween::OnUpdate(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_update_ = callback;
	return *this;
}

Tween& Tween::OnStart(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_start_ = callback;
	return *this;
}

Tween& Tween::OnComplete(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_complete_ = callback;
	return *this;
}

Tween& Tween::OnStop(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_stop_ = callback;
	return *this;
}

Tween& Tween::OnPause(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_pause_ = callback;
	return *this;
}

Tween& Tween::OnResume(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_resume_ = callback;
	return *this;
}

Tween& Tween::OnRepeat(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_repeat_ = callback;
	return *this;
}

Tween& Tween::OnYoyo(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid(), "Cannot set tween callback of uninitialized or destroyed tween");
	instance_->GetLastTweenPoint().on_yoyo_ = callback;
	return *this;
}

} // namespace ptgn
