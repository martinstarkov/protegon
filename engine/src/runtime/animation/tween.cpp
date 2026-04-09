#include "runtime/animation/tween.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/easing.h"
#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "core/time/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

const TweenPoint& TweenData::GetCurrentTweenPoint() const {
	auto current_index{ GetCurrentIndex() };
	PTGN_ASSERT(current_index.has_value());
	PTGN_ASSERT(*current_index < points_.size());
	PTGN_ASSERT(points_[*current_index]);
	return *points_[*current_index];
}

TweenPoint& TweenData::GetCurrentTweenPoint() {
	return const_cast<TweenPoint&>(std::as_const(*this).GetCurrentTweenPoint()); // NOSONAR
}

const TweenPoint& TweenData::GetLastTweenPoint() const {
	auto last_index{ GetLastIndex() };
	PTGN_ASSERT(last_index.has_value());
	PTGN_ASSERT(*last_index < points_.size());
	PTGN_ASSERT(points_[*last_index]);
	return *points_[*last_index];
}

TweenPoint& TweenData::GetLastTweenPoint() {
	return const_cast<TweenPoint&>(std::as_const(*this).GetLastTweenPoint()); // NOSONAR
}

void TweenData::OnEvent() {
	auto point_count{ points_.size() };
	for (std::size_t i{ 0 }; i < point_count; ++i) {
		PTGN_ASSERT(i < points_.size(), "Tween points cannot be shrunk while looping through them");

		PTGN_ASSERT(points_[i]);

		// We deference the unique pointer here because OnEvent can grow tween points safely.
		auto& point{ *points_[i] };

		if (point.flagged_for_removal_) {
			continue;
		}

		// Move pending events so they do not conflict.
		auto current{ std::exchange(point.events_.pending_, {}) };

		for (auto& event : current) {
			EventDispatcher dispatcher{ event };

			PTGN_ASSERT(!event.entity.has_value());

			point.script_container_.OnEvent(dispatcher);
		}
	}
}

void TweenData::ApplyPending() const {
	for (const auto& point : points_) {
		PTGN_ASSERT(point);
		if (point->flagged_for_removal_) {
			continue;
		}
		point->script_container_.ApplyPending();
	}
}

void TweenData::ClearFlagged() {
	std::erase_if(points_, [](const auto& point) {
		PTGN_ASSERT(point);
		return point->flagged_for_removal_;
	});

	if (points_.empty()) {
		index_ = 0;
		return;
	}

	// Since all flagged points are removed, we can be sure that the last index is not flagged.
	if (index_ >= points_.size()) {
		index_ = points_.size() - 1;
	}
}

milliseconds TweenData::GetTotalDuration() const {
	milliseconds total{ 0 };
	for (const auto& point : points_) {
		PTGN_ASSERT(point);
		if (point->flagged_for_removal_) {
			continue;
		}
		total += point->duration_;
	}
	return total;
}

void TweenData::Clear() const {
	for (const auto& point : points_) {
		PTGN_ASSERT(point);
		point->flagged_for_removal_ = true;
	}
}

std::optional<bool> TweenData::FutureTweenPointIsValid() const {
	auto current_index{ GetCurrentIndex() };

	if (!current_index.has_value()) {
		return std::nullopt;
	}

	while (*current_index + 1 < points_.size()) {
		if (points_[*current_index + 1]->flagged_for_removal_) {
			++(*current_index);
		} else {
			return true;
		}
	}

	return false;
}

std::size_t TweenData::GetValidPointCount() const {
	return std::ranges::count_if(points_, [](const auto& point) {
		PTGN_ASSERT(point);
		return !point->flagged_for_removal_;
	});
}

bool TweenData::IsEmpty() const {
	return GetValidPointCount() == 0;
}

TweenPoint& TweenData::EmplaceTweenPoint() {
	const auto& point{ points_.emplace_back(std::make_unique<TweenPoint>()) };
	PTGN_ASSERT(point);
	return *point;
}

void TweenData::IncrementIndex() {
	++index_;
	GetCurrentIndex();
}

void TweenData::RemoveLastTweenPoint() {
	if (points_.empty()) {
		return;
	}

	auto last_index{ GetLastIndex() };

	if (!last_index.has_value()) {
		return;
	}

	PTGN_ASSERT(*last_index < points_.size());

	const auto& last_point{ points_[*last_index] };

	PTGN_ASSERT(last_point);
	last_point->flagged_for_removal_ = true;

	// While index is valid, we keep backing up until we find a valid index or exhaust all indices.
	// This ensures that if the current tween point is the one being removed, we move back to a
	// valid tween point instead of leaving the tween in an invalid state with an invalid current
	// index.
	while (index_ > 0) {
		if (index_ < *last_index) {
			PTGN_ASSERT(index_ < points_.size());
			if (!points_[index_]->flagged_for_removal_) {
				break;
			}
		}

		--index_;
	}
}

std::optional<std::size_t> TweenData::GetCurrentIndex() const {
	while (index_ < points_.size()) {
		PTGN_ASSERT(points_[index_]);

		if (!points_[index_]->flagged_for_removal_) {
			return index_;
		}

		++index_;
	}

	return std::nullopt;
}

std::optional<std::size_t> TweenData::GetLastIndex() const {
	for (std::size_t i{ points_.size() }; i > 0; --i) {
		PTGN_ASSERT(points_[i - 1]);
		if (!points_[i - 1]->flagged_for_removal_) {
			return i - 1;
		}
	}

	return std::nullopt;
}

void TweenData::Reset() {
	progress_ = 0.0f;
	index_	  = 0;
	state_	  = impl::TweenState::Stopped;
	for (const auto& point : points_) {
		PTGN_ASSERT(point);
		if (point->flagged_for_removal_) {
			continue;
		}
		point->current_repeat_	   = 0;
		point->currently_reversed_ = point->start_reversed_;
	}
}

} // namespace impl

bool TweenPoint::IsInfinite() const {
	return !total_repeats_.has_value();
}

bool TweenPoint::IsInstant() const {
	return duration_ == 0ms;
}

Tween::Tween(Entity entity) : Entity{ entity } {}

Tween& Tween::OnProgress(const Tween::Callback<event::TweenProgress>& callback) {
	return AddScript<impl::TweenProgressScript>(callback);
}

Tween& Tween::OnStart(const Tween::Callback<event::TweenStart>& callback) {
	return AddScript<impl::TweenStartScript>(callback);
}

Tween& Tween::OnComplete(const Tween::Callback<event::TweenComplete>& callback) {
	return AddScript<impl::TweenCompleteScript>(callback);
}

Tween& Tween::OnPointStart(const Tween::Callback<event::TweenPointStart>& callback) {
	return AddScript<impl::TweenPointStartScript>(callback);
}

Tween& Tween::OnPointComplete(const Tween::Callback<event::TweenPointComplete>& callback) {
	return AddScript<impl::TweenPointCompleteScript>(callback);
}

Tween& Tween::OnReset(const Tween::Callback<event::TweenReset>& callback) {
	return AddScript<impl::TweenResetScript>(callback);
}

Tween& Tween::OnStop(const Tween::Callback<event::TweenStop>& callback) {
	return AddScript<impl::TweenStopScript>(callback);
}

Tween& Tween::OnPause(const Tween::Callback<event::TweenPause>& callback) {
	return AddScript<impl::TweenPauseScript>(callback);
}

Tween& Tween::OnResume(const Tween::Callback<event::TweenResume>& callback) {
	return AddScript<impl::TweenResumeScript>(callback);
}

Tween& Tween::OnYoyo(const Tween::Callback<event::TweenYoyo>& callback) {
	return AddScript<impl::TweenYoyoScript>(callback);
}

Tween& Tween::OnRepeat(const Tween::Callback<event::TweenRepeat>& callback) {
	return AddScript<impl::TweenRepeatScript>(callback);
}

bool Tween::IsCompleted() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.state_ == impl::TweenState::Completed;
}

bool Tween::IsRunning() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.state_ == impl::TweenState::Started;
}

bool Tween::IsStarted() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.state_ == impl::TweenState::Started || tween.state_ == impl::TweenState::Paused;
}

bool Tween::IsPaused() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.state_ == impl::TweenState::Paused;
}

Tween& Tween::During(milliseconds duration) {
	PTGN_ASSERT(duration >= 0ms, "Tween duration cannot be negative");
	auto& tween{ Get<impl::TweenData>() };
	auto& point{ tween.EmplaceTweenPoint() };
	point.duration_ = duration;
	return *this;
}

Tween& Tween::Start(bool force) {
	if (!force && IsRunning()) {
		return *this;
	}

	Reset();

	auto& tween{ Get<impl::TweenData>() };
	tween.state_ = impl::TweenState::Started;

	auto parent{ GetParent(*this) };

	PushEventToAllTweenPoints<event::TweenStart>(*this, parent);
	PushEventToCurrentTweenPoint<event::TweenPointStart>(*this, parent);

	return *this;
}

Tween& Tween::Stop() {
	if (IsStarted() || IsPaused()) {
		auto& tween{ Get<impl::TweenData>() };
		tween.state_ = impl::TweenState::Stopped;
		PushEventToAllTweenPoints<event::TweenStop>(*this, GetParent(*this));
	}
	return *this;
}

Tween& Tween::Pause() {
	if (!IsRunning()) {
		return *this;
	}
	auto& tween{ Get<impl::TweenData>() };
	tween.state_ = impl::TweenState::Paused;
	PushEventToAllTweenPoints<event::TweenPause>(*this, GetParent(*this));
	return *this;
}

Tween& Tween::Resume() {
	if (!IsPaused()) {
		return *this;
	}
	auto& tween{ Get<impl::TweenData>() };
	tween.state_ = impl::TweenState::Started;
	PushEventToAllTweenPoints<event::TweenResume>(*this, GetParent(*this));
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
	auto& tween{ Get<impl::TweenData>() };
	tween.Reset();
	if (was_started_or_completed) {
		PushEventToAllTweenPoints<event::TweenReset>(*this, GetParent(*this));
	}
	return *this;
}

Tween& Tween::Clear() {
	const auto& tween{ Get<impl::TweenData>() };
	tween.Clear();
	Reset();
	return *this;
}

Tween& Tween::Ease(ptgn::Ease ease) {
	GetLastTweenPoint().ease_ = ease;
	return *this;
}

Tween& Tween::Repeat(std::optional<std::size_t> repeats) {
	bool infinite{ !repeats.has_value() };

	if (!infinite && *repeats == 0) {
		return *this;
	}
	PTGN_ASSERT(infinite || *repeats > 0, "Repeats cannot be negative");

	auto& total_repeats{ GetLastTweenPoint().total_repeats_ };
	total_repeats = repeats;
	if (!infinite) {
		// +1 because the first pass is not counted as a repeat.
		total_repeats.value() += 1;
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
	const auto& tween{ Get<impl::TweenData>() };
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

std::size_t Tween::GetRepeats() const {
	const auto& point{ GetCurrentTweenPoint() };
	return point.current_repeat_;
}

std::optional<std::size_t> Tween::GetCurrentIndex() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.GetCurrentIndex();
}

Tween& Tween::SetDuration(milliseconds duration) {
	GetCurrentTweenPoint().duration_ = duration;
	return *this;
}

milliseconds Tween::GetDuration() const {
	return GetCurrentTweenPoint().duration_;
}

void Tween::Step(secondsf dt) {
	auto& tween{ Get<impl::TweenData>() };

	if (dt <= 0s || tween.state_ != impl::TweenState::Started) {
		return;
	}

	auto parent{ GetParent(*this) };

	if (tween.IsEmpty()) {
		tween.state_ = impl::TweenState::Completed;
		PushEventToAllTweenPoints<event::TweenComplete>(*this, parent);
		return;
	}

	while (dt > 0s && tween.state_ == impl::TweenState::Started) {
		TweenPoint& point{ GetCurrentTweenPoint() };

		if (auto duration{ duration_cast<secondsf>(point.duration_) }; duration <= 0s) {
			tween.progress_ = 1.0f;
			dt				= 0s;
		} else {
			float progress_inc{ dt.count() / duration.count() };
			float new_progress{ tween.progress_ + progress_inc };

			if (new_progress >= 1.0f) {
				dt				= (new_progress - 1.0f) * duration;
				tween.progress_ = 1.0f;
			} else {
				tween.progress_ = new_progress;
				dt				= 0s;
			}
		}

		PushEventToCurrentTweenPoint<event::TweenProgress>(*this, parent, GetProgress());

		if (tween.progress_ >= 1.0f) {
			if (tween.IsEmpty()) {
				continue;
			}

			point.current_repeat_++;

			bool infinite_repeat{ !point.total_repeats_.has_value() };
			bool should_repeat = infinite_repeat || point.current_repeat_ < *point.total_repeats_;

			if (point.yoyo_ && should_repeat) {
				point.currently_reversed_ = !point.currently_reversed_;
				tween.progress_			  = 0.0f;
				PushEventToCurrentTweenPoint<event::TweenYoyo>(*this, parent);
				continue;
			}

			if (should_repeat) {
				tween.progress_ = 0.0f;
				PushEventToCurrentTweenPoint<event::TweenRepeat>(*this, parent);
				continue;
			}

			IncrementPoint();
		}
	}
}

Tween& Tween::IncrementPoint() {
	auto& tween{ Get<impl::TweenData>() };
	if (tween.IsEmpty()) {
		return *this;
	}

	auto future_tween_available{ tween.FutureTweenPointIsValid() };

	// No valid current index, meaning all remaining tween points are flagged for removal.
	if (!future_tween_available.has_value()) {
		return *this;
	}

	auto parent{ GetParent(*this) };

	if (*future_tween_available) {
		// Move to next tween point.
		PushEventToCurrentTweenPoint<event::TweenPointComplete>(*this, parent);
		tween.IncrementIndex();
		PushEventToCurrentTweenPoint<event::TweenPointStart>(*this, parent);
		tween.progress_ = 0.0f;

		// Reset repeat count and reversal
		TweenPoint& new_point		  = GetCurrentTweenPoint();
		new_point.current_repeat_	  = 0;
		new_point.currently_reversed_ = new_point.start_reversed_;
		return *this;
	}

	// Final tween point completed, complete tween.
	if (tween.state_ != impl::TweenState::Completed) {
		PushEventToCurrentTweenPoint<event::TweenPointComplete>(*this, parent);
	}
	// No more points: complete
	tween.state_	= impl::TweenState::Completed;
	tween.progress_ = 1.0f;
	PushEventToAllTweenPoints<event::TweenComplete>(*this, parent);
	return *this;
}

Tween& Tween::RemoveLastTweenPoint() {
	auto& tween{ Get<impl::TweenData>() };
	tween.RemoveLastTweenPoint();
	return *this;
}

void Tween::Seek(float new_progress) {
	new_progress = Clamp01(new_progress);

	Reset(); // Reset and re-simulate from 0.
	Start();

	float current_progress{ 0.0f };
	float target_progress{ new_progress };

	constexpr float step_size{ 1.0f / 120.0f }; // simulate ~120 fps (or use config value)

	const auto& tween{ Get<impl::TweenData>() };

	while (current_progress < target_progress && !IsCompleted()) {
		float before{ tween.progress_ };

		Step(secondsf{ step_size });

		current_progress = tween.progress_;

		// Avoid infinite loop on broken tweens
		if (std::abs(current_progress - before) < kEpsilon<float>) {
			break;
		}
	}
}

void Tween::Seek(milliseconds time) {
	float total_ms{ duration_cast<millisecondsf>(GetTotalDuration()).count() };
	PTGN_ASSERT(total_ms > 0.0f, "Cannot seek tween when total duration is 0");
	float target_progress{ duration_cast<millisecondsf>(time).count() / total_ms };
	Seek(target_progress);

	// Alternative implementation.
	// float target_time{ duration_cast<secondsf>(time).count() };
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
	const auto& tween{ Get<impl::TweenData>() };
	return tween.GetValidPointCount();
}

milliseconds Tween::GetTotalDuration() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.GetTotalDuration();
}

const TweenPoint& Tween::GetCurrentTweenPoint() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.GetCurrentTweenPoint();
}

TweenPoint& Tween::GetCurrentTweenPoint() {
	auto& tween{ Get<impl::TweenData>() };
	return tween.GetCurrentTweenPoint();
}

const TweenPoint& Tween::GetLastTweenPoint() const {
	const auto& tween{ Get<impl::TweenData>() };
	return tween.GetLastTweenPoint();
}

TweenPoint& Tween::GetLastTweenPoint() {
	auto& tween{ Get<impl::TweenData>() };
	return tween.GetLastTweenPoint();
}

void Tween::Update(Scene& scene, secondsf dt) {
	for (auto [entity, tween] : scene.EntitiesWith<impl::TweenData>()) {
		tween.ApplyPending();
	}
	for (auto [entity, tween] : scene.EntitiesWith<impl::TweenData>()) {
		Tween{ entity }.Step(dt);
	}
	for (auto [entity, tween] : scene.EntitiesWith<impl::TweenData>()) {
		tween.OnEvent();
	}
	for (auto [entity, tween] : scene.EntitiesWith<impl::TweenData>()) {
		tween.ClearFlagged();
	}
}

std::ostream& operator<<(std::ostream& os, impl::TweenState state) {
	switch (state) {
		using enum impl::TweenState;
		case Stopped:	return os << "Stopped";
		case Started:	return os << "Started";
		case Paused:	return os << "Paused";
		case Completed: return os << "Completed";
		default:		PTGN_ERROR("Unknown TweenState: ", std::to_underlying(state));
	}
}

Tween CreateTween(Scene& scene) {
	Tween tween{ scene.CreateEntity() };

	tween.Add<impl::TweenData>();

	return tween;
}

} // namespace ptgn