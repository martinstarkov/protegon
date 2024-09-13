#include "protegon/tween.h"

namespace ptgn {

namespace impl {

template <typename T, typename... TArgs>
inline void InvokeCallback(const TweenCallback& callback, TArgs&&... args) {
	auto& f = std::get<T>(callback);
	if (f) {
		std::invoke(f, std::forward<TArgs>(args)...);
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
		tweens_points_.size() > 0, "TweenInstance must be given duration before setting properties"
	);
	return tweens_points_.back();
}

TweenInstance::TweenInstance(milliseconds duration) {
	During(duration);
}

void TweenInstance::During(milliseconds duration) {
	PTGN_ASSERT(duration > nanoseconds{ 250 });
	tweens_points_.emplace_back(duration);
}

void TweenInstance::Start() {
	Reset();
	started_ = true;
	ActivateCallback(GetCurrentTweenPoint().on_start_);
}

void TweenInstance::Reset() {
	if (IsStarted() || IsCompleted()) {
		ActivateCallback(GetCurrentTweenPoint().on_stop_);
	}
	index_	  = 0;
	progress_ = 0.0f;
	started_  = false;
	paused_	  = false;
	for (auto& point : tweens_points_) {
		point.current_repeat_ = 0;
	}
}

void TweenInstance::ActivateDestroyCallback() {
	ActivateCallback(on_destroy_);
}

void TweenInstance::Pause() {
	if (!paused_) {
		paused_ = true;
		ActivateCallback(GetCurrentTweenPoint().on_pause_);
	}
}

void TweenInstance::Resume() {
	if (paused_) {
		paused_ = false;
		ActivateCallback(GetCurrentTweenPoint().on_resume_);
	}
}

/*float TweenInstance::Rewind(float dt) {
	return Step(-dt);
}*/

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

float TweenInstance::Step(float dt) {
	return StepImpl(dt, true);
}

float TweenInstance::StepImpl(float dt, bool accumulate_progress) {
	return SeekImpl(
		accumulate_progress ? AccumulateProgress(GetNewProgress(duration<float>(dt)))
							: GetNewProgress(duration<float>(dt))
	);
}

float TweenInstance::Seek(float new_progress) {
	return SeekImpl(AccumulateProgress(new_progress));
}

float TweenInstance::Seek(milliseconds time) {
	return SeekImpl(AccumulateProgress(GetNewProgress(time)));
}

float TweenInstance::SeekImpl(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	if (!started_ || paused_) {
		return GetProgress();
	}

	progress_ = new_progress;

	return UpdateImpl(false);
}

float TweenInstance::AccumulateProgress(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f);
	PTGN_ASSERT(!std::isnan(new_progress));
	PTGN_ASSERT(!std::isinf(new_progress));

	if (new_progress < 1.0f) {
		return new_progress;
	}

	if (!started_ || paused_) {
		return GetProgress();
	}

	float loops{ std::floorf(new_progress) };

	for (float i = 0; i < loops; i++) {
		progress_ = 1.0f;
		UpdateImpl(true);
		if (IsCompleted()) {
			return 1.0f;
		}
	}

	PTGN_ASSERT(new_progress >= loops);

	new_progress -= loops;

	return new_progress;
}

float TweenInstance::GetProgress() const {
	auto& current{ GetCurrentTweenPoint() };

	float progress = current.reversed_ ? 1.0f - progress_ : progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return std::invoke(current.easing_func_, progress, 0.0f, 1.0f);
}

bool TweenInstance::IsCompleted() const {
	return tweens_points_.size() > 0 && progress_ >= 1.0f &&
		   (index_ >= tweens_points_.size() - 1 || !started_);
}

bool TweenInstance::IsStarted() const {
	return started_;
}

bool TweenInstance::IsPaused() const {
	return paused_;
}

std::int64_t TweenInstance::GetRepeats() const {
	return GetCurrentTweenPoint().current_repeat_;
}

void TweenInstance::Repeat(std::int64_t repeats) {
	PTGN_ASSERT(repeats == -1 || repeats > 0);
	auto& total_repeats{ GetLastTweenPoint().total_repeats_ };
	total_repeats = repeats;
	if (total_repeats != -1) {
		// +1 because the first pass is not counted as a repeat.
		total_repeats += 1;
	}
}

void TweenInstance::Ease(TweenEase ease) {
	auto it = impl::tween_ease_functions_.find(ease);
	PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Could not identify tween easing type");
	GetLastTweenPoint().easing_func_ = it->second;
}

void TweenInstance::Reverse(bool reversed) {
	GetLastTweenPoint().reversed_ = reversed;
}

void TweenInstance::Yoyo(bool yoyo) {
	GetLastTweenPoint().yoyo_ = yoyo;
}

void TweenInstance::Forward() {
	Reverse(false);
}

void TweenInstance::Backward() {
	Reverse(true);
}

void TweenInstance::Clear() {
	Reset();
	tweens_points_.clear();
}

void TweenInstance::Complete() {
	Seek(GetCurrentTweenPoint().reversed_ ? 0.0f : 1.0f);
}

void TweenInstance::Stop() {
	if (started_) {
		ActivateCallback(GetCurrentTweenPoint().on_stop_);
		started_ = false;
	}
}

void TweenInstance::ActivateCallback(const TweenCallback& callback) {
	if (std::holds_alternative<std::function<void()>>(callback)) {
		InvokeCallback<std::function<void()>>(callback);
	} else if (std::holds_alternative<std::function<void(float)>>(callback)) {
		InvokeCallback<std::function<void(float)>>(callback, GetProgress());
	} else if (std::holds_alternative<std::function<void(Tween)>>(callback)) {
		InvokeCallback<std::function<void(Tween)>>(callback, std::move(Tween{ getptr() }));
	} else if (std::holds_alternative<std::function<void(Tween, float)>>(callback)) {
		InvokeCallback<std::function<void(Tween, float)>>(
			callback, std::move(Tween{ getptr() }), GetProgress()
		);
	} else {
		PTGN_ERROR("Failed to identify tween callback function");
	}
}

void TweenInstance::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(duration > nanoseconds{ 250 });
	PTGN_ASSERT(
		tween_point_index < tweens_points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	tweens_points_[tween_point_index].duration_ = duration;
	UpdateImpl();
}

void TweenInstance::PointCompleted() {
	ActivateCallback(GetCurrentTweenPoint().on_complete_);
	if (index_ < tweens_points_.size() - 1) {
		index_++;
		progress_ = 0.0f;
		ActivateCallback(GetCurrentTweenPoint().on_start_);
	} else {
		progress_ = 1.0f;
		started_  = false;
	}
}

void TweenInstance::HandleCallbacks(bool suppress_update) {
	if (!started_ || paused_) {
		return;
	}

	auto& current{ GetCurrentTweenPoint() };

	if (!suppress_update) {
		ActivateCallback(current.on_update_);
	}

	PTGN_ASSERT(progress_ <= 1.0f);

	// TweenInstance has not reached end of repetition.
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
		current.reversed_ = !current.reversed_;
		ActivateCallback(current.on_yoyo_);
	}

	// Repeat the tween.
	progress_ = 0.0f;
	ActivateCallback(current.on_repeat_);
}

float TweenInstance::UpdateImpl(bool suppress_update) {
	PTGN_ASSERT(progress_ <= 1.0f);

	auto& current{ GetCurrentTweenPoint() };

	if (progress_ >= 1.0f &&
		(current.current_repeat_ < current.total_repeats_ || current.total_repeats_ == -1)) {
		current.current_repeat_++;
	}

	HandleCallbacks(suppress_update);

	return GetProgress();
}

void TweenInstance::OnUpdate(const TweenCallback& callback) {
	GetLastTweenPoint().on_update_ = callback;
}

void TweenInstance::OnStart(const TweenCallback& callback) {
	GetLastTweenPoint().on_start_ = callback;
}

void TweenInstance::OnComplete(const TweenCallback& callback) {
	GetLastTweenPoint().on_complete_ = callback;
}

void TweenInstance::OnStop(const TweenCallback& callback) {
	GetLastTweenPoint().on_stop_ = callback;
}

void TweenInstance::OnPause(const TweenCallback& callback) {
	GetLastTweenPoint().on_pause_ = callback;
}

void TweenInstance::OnResume(const TweenCallback& callback) {
	GetLastTweenPoint().on_resume_ = callback;
}

void TweenInstance::OnRepeat(const TweenCallback& callback) {
	GetLastTweenPoint().on_repeat_ = callback;
}

void TweenInstance::OnYoyo(const TweenCallback& callback) {
	GetLastTweenPoint().on_yoyo_ = callback;
}

void TweenInstance::OnDestroy(const TweenCallback& callback) {
	on_destroy_ = callback;
}

} // namespace impl

Tween::Tween(milliseconds duration) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TweenInstance>(duration);
	}
}

Tween& Tween::During(milliseconds duration) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::TweenInstance>();
	}
	instance_->During(duration);
	return *this;
}

Tween& Tween::Ease(TweenEase ease) {
	PTGN_ASSERT(IsValid());
	instance_->Ease(ease);
	return *this;
}

Tween& Tween::Repeat(std::int64_t repeats) {
	PTGN_ASSERT(IsValid());
	instance_->Repeat(repeats);
	return *this;
}

Tween& Tween::Reverse(bool reversed) {
	PTGN_ASSERT(IsValid());
	instance_->Reverse(reversed);
	return *this;
}

Tween& Tween::Yoyo(bool yoyo) {
	PTGN_ASSERT(IsValid());
	instance_->Yoyo(yoyo);
	return *this;
}

Tween& Tween::OnUpdate(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnUpdate(callback);
	return *this;
}

Tween& Tween::OnStart(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnStart(callback);
	return *this;
}

Tween& Tween::OnComplete(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnComplete(callback);
	return *this;
}

Tween& Tween::OnStop(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnStop(callback);
	return *this;
}

Tween& Tween::OnPause(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnPause(callback);
	return *this;
}

Tween& Tween::OnResume(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnResume(callback);
	return *this;
}

Tween& Tween::OnRepeat(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnRepeat(callback);
	return *this;
}

Tween& Tween::OnYoyo(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnYoyo(callback);
	return *this;
}

Tween& Tween::OnDestroy(const TweenCallback& callback) {
	PTGN_ASSERT(IsValid());
	instance_->OnDestroy(callback);
	return *this;
}

float Tween::GetProgress() const {
	PTGN_ASSERT(IsValid());
	return instance_->GetProgress();
}

std::int64_t Tween::GetRepeats() const {
	PTGN_ASSERT(IsValid());
	return instance_->GetRepeats();
}

bool Tween::IsCompleted() const {
	return IsValid() && instance_->IsCompleted();
}

bool Tween::IsStarted() const {
	return IsValid() && instance_->IsStarted();
}

bool Tween::IsPaused() const {
	return IsValid() && instance_->IsPaused();
}

float Tween::Step(float dt) {
	PTGN_ASSERT(IsValid());
	return instance_->Step(dt);
}

float Tween::Seek(float new_progress) {
	PTGN_ASSERT(IsValid());
	return instance_->Seek(new_progress);
}

float Tween::Seek(milliseconds time) {
	PTGN_ASSERT(IsValid());
	return instance_->Seek(time);
}

Tween& Tween::Start() {
	PTGN_ASSERT(IsValid());
	instance_->Start();
	return *this;
}

Tween& Tween::Pause() {
	PTGN_ASSERT(IsValid());
	instance_->Pause();
	return *this;
}

void Tween::Resume() {
	PTGN_ASSERT(IsValid());
	instance_->Resume();
}

void Tween::Reset() {
	PTGN_ASSERT(IsValid());
	instance_->Reset();
}

void Tween::Stop() {
	PTGN_ASSERT(IsValid());
	instance_->Stop();
}

void Tween::Destroy() {
	if (!IsValid()) {
		return;
	}
	if (instance_->IsStarted()) {
		Stop();
	}
	instance_->ActivateDestroyCallback();
	instance_ = nullptr;
}

void Tween::Complete() {
	PTGN_ASSERT(IsValid());
	instance_->Complete();
}

void Tween::Forward() {
	PTGN_ASSERT(IsValid());
	instance_->Forward();
}

void Tween::Backward() {
	PTGN_ASSERT(IsValid());
	instance_->Backward();
}

void Tween::Clear() {
	PTGN_ASSERT(IsValid());
	instance_->Clear();
}

void Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(IsValid());
	instance_->SetDuration(duration, tween_point_index);
}

Tween::Tween(std::shared_ptr<impl::TweenInstance> instance) {
	PTGN_ASSERT(!IsValid());
	instance_ = instance;
}

} // namespace ptgn
