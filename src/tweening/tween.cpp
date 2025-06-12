#include "tweening/tween.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "core/entity.h"
#include "core/script.h"
#include "core/time.h"
#include "math/easing.h"
#include "scene/scene.h"

#define PTGN_CALL_TWEEN_SCRIPTS(FUNC_NAME)                                         \
	auto& FUNC_NAME##_scripts{ GetCurrentTweenPoint().script_container_.scripts }; \
	for (auto& [key, script] : FUNC_NAME##_scripts) {                              \
		script->FUNC_NAME({ *this, GetProgress(), Entity::GetParent() });          \
	}

namespace ptgn {

Tween CreateTween(Scene& scene) {
	Tween tween{ scene.CreateEntity() };

	tween.Add<impl::TweenInstance>();

	return tween;
}

namespace impl {

TweenPoint::TweenPoint(milliseconds duration) : duration_{ duration } {}

void TweenPoint::SetReversed(bool reversed) {
	start_reversed_		= reversed;
	currently_reversed_ = start_reversed_;
}

TweenInstance::TweenInstance(TweenInstance&& other) noexcept :
	progress_{ std::exchange(other.progress_, 0.0f) },
	index_{ std::exchange(other.index_, 0) },
	points_{ std::exchange(other.points_, {}) },
	paused_{ std::exchange(other.paused_, false) },
	started_{ std::exchange(other.started_, false) } {}

TweenInstance& TweenInstance::operator=(TweenInstance&& other) noexcept {
	if (this != &other) {
		progress_ = std::exchange(other.progress_, 0.0f);
		index_	  = std::exchange(other.index_, 0);
		points_	  = std::exchange(other.points_, {});
		paused_	  = std::exchange(other.paused_, false);
		started_  = std::exchange(other.started_, false);
	}
	return *this;
}

TweenInstance::~TweenInstance() {
	progress_ = 0.0f;
	index_	  = 0;
	points_.clear();
	points_.shrink_to_fit();
	paused_	 = false;
	started_ = false;
}

} // namespace impl

Tween::Tween(const Entity& entity) : Entity{ entity } {}

bool Tween::IsCompleted() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return !tween.points_.empty() && tween.progress_ >= 1.0f &&
		   (tween.index_ >= tween.points_.size() - 1 || !tween.started_);
}

float Tween::GetNewProgress(duration<float> time) const {
	duration<float> progress{
		time / std::chrono::duration_cast<duration<float>>(GetCurrentTweenPoint().duration_)
	};
	float p{ progress.count() };
	if (std::isinf(p) || std::isnan(p)) {
		return 1.0f;
	}
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.progress_ + p;
}

float Tween::GetProgress() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	if (tween.points_.empty()) {
		return tween.progress_;
	}

	auto& current{ GetCurrentTweenPoint() };

	float progress = current.currently_reversed_ ? 1.0f - tween.progress_ : tween.progress_;

	PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f, "Progress updating failed");

	return ApplyEase(progress, current.ease_);
}

const impl::TweenPoint& Tween::GetCurrentTweenPoint() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(!tween.points_.empty());
	PTGN_ASSERT(tween.index_ <= tween.points_.size());
	if (tween.index_ == tween.points_.size()) {
		return tween.points_.back();
	}
	return tween.points_[tween.index_];
}

impl::TweenPoint& Tween::GetCurrentTweenPoint() {
	return const_cast<impl::TweenPoint&>(std::as_const(*this).GetCurrentTweenPoint());
}

impl::TweenPoint& Tween::GetLastTweenPoint() {
	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(!tween.points_.empty(), "Tween must be given duration before setting properties");
	return tween.points_.back();
}

Tween& Tween::During(milliseconds duration) {
	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");
	tween.points_.emplace_back(duration);
	return *this;
}

float Tween::Step(float dt) {
	return StepImpl(dt, true);
}

float Tween::Seek(float new_progress) {
	auto& tween{ Get<impl::TweenInstance>() };
	if (!tween.started_ || tween.paused_ || tween.points_.empty()) {
		return GetProgress();
	}
	return SeekImpl(AccumulateProgress(new_progress));
}

float Tween::Seek(milliseconds time) {
	auto& tween{ Get<impl::TweenInstance>() };
	if (!tween.started_ || tween.paused_ || tween.points_.empty()) {
		return GetProgress();
	}
	return SeekImpl(AccumulateProgress(GetNewProgress(time)));
}

bool Tween::IsRunning() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.started_ && !tween.paused_;
}

bool Tween::IsStarted() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.started_;
}

bool Tween::IsPaused() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.paused_;
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

Tween& Tween::Ease(const ptgn::Ease& ease) {
	GetLastTweenPoint().ease_ = ease;
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
	auto& tween{ Get<impl::TweenInstance>() };
	tween.points_.clear();
	return *this;
}

milliseconds Tween::GetDuration(std::size_t tween_point_index) const {
	const auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(
		tween_point_index < tween.points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	return tween.points_[tween_point_index].duration_;
}

Tween& Tween::SetDuration(milliseconds duration, std::size_t tween_point_index) {
	PTGN_ASSERT(duration >= nanoseconds{ 0 }, "Tween duration cannot be negative");

	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(
		tween_point_index < tween.points_.size(),
		"Specified tween point index is out of range. Ensure tween points has been added "
		"beforehand"
	);
	tween.points_[tween_point_index].duration_ = duration;
	UpdateImpl();
	return *this;
}

void Tween::PointCompleted() {
	auto& tween{ Get<impl::TweenInstance>() };
	if (tween.points_.empty()) {
		return;
	}
	PTGN_CALL_TWEEN_SCRIPTS(OnComplete);
	tween = Get<impl::TweenInstance>();
	if (!tween.points_.empty() && tween.index_ < tween.points_.size() - 1) {
		tween.index_++;
		tween.progress_							   = 0.0f;
		GetCurrentTweenPoint().currently_reversed_ = GetCurrentTweenPoint().start_reversed_;
		if (tween.started_) {
			PTGN_CALL_TWEEN_SCRIPTS(OnStart);
		}
	} else {
		tween.progress_ = 1.0f;
		tween.started_	= false;
	}
}

void Tween::HandleCallbacks(bool suppress_update) {
	if (!IsStarted() || IsPaused()) {
		return;
	}

	if (!suppress_update) {
		PTGN_CALL_TWEEN_SCRIPTS(OnUpdate);
	}

	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(tween.progress_ <= 1.0f);

	// Tween has not reached end of repetition.
	if (tween.progress_ < 1.0f) {
		return;
	}

	auto& current{ GetCurrentTweenPoint() };
	// Completed tween.
	if (current.current_repeat_ == current.total_repeats_) {
		if (suppress_update) {
			PTGN_CALL_TWEEN_SCRIPTS(OnUpdate);
		}
		PointCompleted();
		return;
	}

	// Reverse yoyoing tweens.
	if (current.yoyo_) {
		current.currently_reversed_ = !current.currently_reversed_;
		PTGN_CALL_TWEEN_SCRIPTS(OnYoyo);
	}

	tween = Get<impl::TweenInstance>();
	// Repeat the tween.
	tween.progress_ = 0.0f;
	PTGN_CALL_TWEEN_SCRIPTS(OnRepeat);
}

float Tween::UpdateImpl(bool suppress_update) {
	auto& tween{ Get<impl::TweenInstance>() };
	PTGN_ASSERT(tween.progress_ <= 1.0f);

	if (auto& current{ GetCurrentTweenPoint() };
		tween.progress_ >= 1.0f &&
		(current.current_repeat_ < current.total_repeats_ || current.total_repeats_ == -1)) {
		current.current_repeat_++;
	}

	HandleCallbacks(suppress_update);

	tween = Get<impl::TweenInstance>();
	// After completion and destruction.
	if (!tween.started_ && tween.progress_ == 1.0f) {
		return 1.0f;
	}

	return GetProgress();
}

Tween& Tween::Pause() {
	auto& tween{ Get<impl::TweenInstance>() };
	if (!tween.paused_) {
		tween.paused_ = true;
		if (!tween.points_.empty()) {
			PTGN_CALL_TWEEN_SCRIPTS(OnPause);
		}
	}
	return *this;
}

Tween& Tween::Resume() {
	auto& tween{ Get<impl::TweenInstance>() };
	if (tween.paused_) {
		tween.paused_ = false;
		if (!tween.points_.empty()) {
			PTGN_CALL_TWEEN_SCRIPTS(OnResume);
		}
	}
	return *this;
}

Tween& Tween::Reset() {
	auto& tween{ Get<impl::TweenInstance>() };
	if (tween.started_ || IsCompleted()) {
		// Reset all tween points, not just the current one.
		for (auto& tween_point : tween.points_) {
			auto& scripts{ tween_point.script_container_.scripts };
			for (auto& [key, script] : scripts) {
				script->OnReset({ *this, GetProgress(), Entity::GetParent() });
			}
		}
	}
	tween			= Get<impl::TweenInstance>();
	tween.index_	= 0;
	tween.progress_ = 0.0f;
	tween.started_	= false;
	tween.paused_	= false;
	for (auto& point : tween.points_) {
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
	auto& tween{ Get<impl::TweenInstance>() };
	tween.started_ = true;
	if (!tween.points_.empty()) {
		PTGN_CALL_TWEEN_SCRIPTS(OnStart);
	}
	return *this;
}

Tween& Tween::IncrementTweenPoint() {
	if (IsCompleted()) {
		return *this;
	}
	// Cannot increment tween point any further.
	if (auto& tween{ Get<impl::TweenInstance>() }; tween.index_ >= tween.points_.size()) {
		return *this;
	}
	PointCompleted();
	return *this;
}

std::size_t Tween::GetCurrentIndex() const {
	const auto& tween{ Get<impl::TweenInstance>() };
	return tween.index_;
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
	auto& tween{ Get<impl::TweenInstance>() };
	if (tween.started_) {
		if (!tween.points_.empty()) {
			PTGN_CALL_TWEEN_SCRIPTS(OnStop);
		}
		tween		   = Get<impl::TweenInstance>();
		tween.started_ = false;
	}
	return *this;
}

float Tween::StepImpl(float dt, bool accumulate_progress) {
	auto& tween{ Get<impl::TweenInstance>() };
	if (!tween.started_ || tween.paused_ || tween.points_.empty() || IsCompleted()) {
		return GetProgress();
	}
	float new_progress{ GetNewProgress(duration<float>(dt)) };
	return SeekImpl(accumulate_progress ? AccumulateProgress(new_progress) : new_progress);
}

float Tween::SeekImpl(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f && new_progress <= 1.0f, "Progress accumulator failed");

	auto& tween{ Get<impl::TweenInstance>() };
	if (!tween.started_ || tween.paused_ || tween.points_.empty()) {
		return GetProgress();
	}

	tween.progress_ = new_progress;

	return UpdateImpl(false);
}

float Tween::AccumulateProgress(float new_progress) {
	PTGN_ASSERT(new_progress >= 0.0f);
	PTGN_ASSERT(!std::isnan(new_progress));
	PTGN_ASSERT(!std::isinf(new_progress));

	if (new_progress < 1.0f) {
		return new_progress;
	}

	auto& tween{ Get<impl::TweenInstance>() };
	if (!tween.started_ || tween.paused_) {
		return GetProgress();
	}

	std::size_t loops{ static_cast<std::size_t>(std::floorf(new_progress)) };

	for (std::size_t i{ 0 }; i < loops; i++) {
		tween			= Get<impl::TweenInstance>();
		tween.progress_ = 1.0f;
		UpdateImpl(true);
		if (IsCompleted()) {
			return 1.0f;
		}
	}

	PTGN_ASSERT(new_progress >= 0.0f);
	PTGN_ASSERT(static_cast<std::size_t>(new_progress) >= loops);

	new_progress -= static_cast<float>(loops);

	return new_progress;
}

} // namespace ptgn