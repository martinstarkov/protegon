#include "protegon/tween.h"

namespace ptgn {

Tween::Tween(
	TweenType from, TweenType to, milliseconds duration, const TweenConfig& config, bool start
) {
	instance_		   = std::make_shared<impl::TweenInstance>();
	instance_->from_   = from;
	instance_->to_	   = to;
	instance_->config_ = config;
	if (instance_->config_.yoyo) {
		PTGN_ASSERT(instance_->config_.repeat != 0, "Yoyoing a tween requires repeat != 0");
	}
	instance_->duration_ = duration;
	instance_->reversed_ = instance_->config_.reversed;
	instance_->paused_	 = instance_->config_.paused;
	if (start) {
		Start();
	}
}

void Tween::Start() {
	PTGN_ASSERT(IsValid(), "Cannot start uninitialized or destroyed tween");
	Reset();
	instance_->running_ = true;
	ActivateCallback(instance_->config_.on_start, GetValue());
}

void Tween::Pause() {
	PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
	if (!instance_->paused_) {
		instance_->paused_ = true;
		ActivateCallback(instance_->config_.on_pause, GetValue());
	}
}

void Tween::Resume() {
	PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
	if (instance_->paused_) {
		instance_->paused_ = false;
		ActivateCallback(instance_->config_.on_resume, GetValue());
	}
}

TweenType Tween::Rewind(float dt) {
	return Step(-dt);
}

float Tween::GetNewProgress(duration<float, Time::period> time) const {
	PTGN_ASSERT(IsValid(), "Cannot get new progress for uninitialized or destroyed tween");
	PTGN_ASSERT(IsValid(), "Cannot convert time to progress for uninitialized or destroyed tween");
	duration<float, Time::period> progress{
		time / std::chrono::duration_cast<duration<float, Time::period>>(instance_->duration_)
	};
	return instance_->progress_ + progress.count();
}

TweenType Tween::StepImpl(float dt, bool accumulate_progress) {
	PTGN_ASSERT(IsValid(), "Cannot step uninitialized or destroyed tween");

	if (!instance_->running_ || instance_->paused_) {
		return GetValue();
	}

	float new_progress = GetNewProgress(duration<float, seconds::period>(dt));

	if (accumulate_progress) {
		new_progress = AccumulateProgress(new_progress);
	}

	return SeekImpl(new_progress);
}

TweenType Tween::Step(float dt) {
	return StepImpl(dt, true);
}

TweenType Tween::Seek(float new_progress) {
	PTGN_ASSERT(IsValid(), "Cannot seek uninitialized or destroyed tween");

	if (!instance_->running_ || instance_->paused_) {
		return GetValue();
	}

	return SeekImpl(AccumulateProgress(new_progress));
}

float Tween::AccumulateProgress(float new_progress) {
	if (new_progress < 1.0f) {
		return new_progress;
	}

	std::int64_t loops{ static_cast<std::int64_t>(new_progress) };

	for (std::int64_t i = 0; i < loops; i++) {
		instance_->progress_ = 1.0f;
		UpdateImpl(true);
		if (IsCompleted()) {
			return 1.0f;
		}
	}

	new_progress -= loops;

	return new_progress;
}

TweenType Tween::SeekImpl(float new_progress) {
	PTGN_ASSERT(IsValid(), "Cannot seek uninitialized or destroyed tween");
	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	instance_->progress_ = new_progress;

	return UpdateImpl(false);
}

TweenType Tween::Seek(milliseconds time) {
	PTGN_ASSERT(IsValid(), "Cannot seek uninitialized or destroyed tween");

	if (!instance_->running_ || instance_->paused_) {
		return GetValue();
	}

	float new_progress = GetNewProgress(time);

	return SeekImpl(AccumulateProgress(new_progress));
}

float Tween::GetProgress() const {
	PTGN_ASSERT(IsValid(), "Cannot get progress of uninitialized or destroyed tween");

	float progress = instance_->reversed_ ? 1.0f - instance_->progress_ : instance_->progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return progress;
}

TweenType Tween::GetValue() const {
	PTGN_ASSERT(IsValid(), "Cannot get value of uninitialized or destroyed tween");

	auto it = impl::tween_ease_functions_.find(instance_->config_.ease);

	PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Failed to recognize easing type");

	const auto& ease_func = it->second;
	return ease_func(GetProgress(), instance_->from_, instance_->to_);
}

TweenType Tween::GetFromValue() const {
	PTGN_ASSERT(IsValid(), "Cannot get from value of uninitialized or destroyed tween");
	return instance_->from_;
}

TweenType Tween::GetToValue() const {
	PTGN_ASSERT(IsValid(), "Cannot get to value of uninitialized or destroyed tween");
	return instance_->to_;
}

void Tween::SetFromValue(TweenType from) {
	PTGN_ASSERT(IsValid(), "Cannot set from value of uninitialized or destroyed tween");
	instance_->from_ = from;
	UpdateImpl();
}

void Tween::SetToValue(TweenType to) {
	PTGN_ASSERT(IsValid(), "Cannot set to value of uninitialized or destroyed tween");
	instance_->to_ = to;
	UpdateImpl();
}

const TweenConfig& Tween::GetConfig() const {
	PTGN_ASSERT(IsValid(), "Cannot get config of uninitialized or destroyed tween");
	return instance_->config_;
}

bool Tween::IsCompleted() const {
	return IsValid() && instance_->completed_;
}

bool Tween::IsStarted() const {
	return IsValid() && instance_->running_;
}

bool Tween::IsPaused() const {
	return IsValid() && instance_->paused_;
}

std::int64_t Tween::GetRepeats() const {
	PTGN_ASSERT(IsValid(), "Cannot get repeats of uninitialized or destroyed tween");
	return instance_->repeats_;
}

void Tween::SetReversed(bool reversed) {
	PTGN_ASSERT(IsValid(), "Cannot reverse uninitialized or destroyed tween");
	instance_->reversed_ = reversed;
}

void Tween::Forward() {
	SetReversed(false);
}

void Tween::Backward() {
	SetReversed(true);
}

void Tween::Complete() {
	Seek(instance_->reversed_ ? 0.0f : 1.0f);
}

void Tween::Reset() {
	PTGN_ASSERT(IsValid(), "Cannot reset uninitialized or destroyed tween");
	instance_->repeats_	  = 0;
	instance_->progress_  = 0.0f;
	instance_->running_	  = false;
	instance_->completed_ = false;
	instance_->reversed_  = instance_->config_.reversed;
	instance_->paused_	  = instance_->config_.paused;
}

void Tween::Stop() {
	PTGN_ASSERT(IsValid(), "Cannot stop uninitialized or destroyed tween");
	if (instance_->running_) {
		ActivateCallback(instance_->config_.on_stop, GetValue());
		instance_->running_ = false;
		// TODO: Consider destroying tween instance.
		// Destroy();
	}
}

void Tween::Destroy() {
	instance_.reset();
}

void Tween::ActivateCallback(const TweenCallback& callback, TweenType value) {
	if (callback != nullptr) {
		callback(*this, value);
	}
}

void Tween::HandleRepeats() {
	PTGN_ASSERT(instance_->progress_ <= 1.0f);

	if (instance_->progress_ >= 1.0f &&
		(instance_->repeats_ < instance_->config_.repeat || instance_->config_.repeat == -1)) {
		instance_->repeats_++;
	}

	PTGN_ASSERT(
		instance_->repeats_ <= instance_->config_.repeat || instance_->config_.repeat == -1
	);
}

void Tween::HandleCallbacks(TweenType value, bool suppress_update) {
	if (!instance_->running_ || instance_->paused_) {
		return;
	}

	if (!suppress_update) {
		ActivateCallback(instance_->config_.on_update, value);
	}

	PTGN_ASSERT(instance_->progress_ <= 1.0f);

	// Tween has not reached end of repetition.
	if (instance_->progress_ < 1.0f) {
		return;
	}

	// Completed tween.
	if (instance_->repeats_ == instance_->config_.repeat) {
		instance_->running_	  = false;
		instance_->completed_ = true;
		if (suppress_update) {
			ActivateCallback(instance_->config_.on_update, value);
		}
		ActivateCallback(instance_->config_.on_complete, value);
		return;
	}

	// Reverse yoyoing tweens.
	if (instance_->config_.yoyo) {
		SetReversed(!instance_->reversed_);
		ActivateCallback(instance_->config_.on_yoyo, value);
	}

	// Repeat the tween.
	instance_->progress_ = 0.0f;
	ActivateCallback(instance_->config_.on_repeat, value);
}

TweenType Tween::UpdateImpl(bool suppress_update) {
	HandleRepeats();
	auto value{ GetValue() };
	HandleCallbacks(value, suppress_update);
	return value;
}

} // namespace ptgn