#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <variant>
#include <vector>

#include "core/assert.h"

#include "core/math/easing.h"
#include "core/time/time.h"
#include "runtime/ecs/entity.h"

#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class ScriptSequence;
class TweenPoint;

namespace impl {

class TweenData;

} // namespace impl

namespace event {

struct TweenProgress;
struct TweenComplete;
struct TweenPointStart;
struct TweenPointComplete;
struct TweenReset;
struct TweenStart;
struct TweenStop;
struct TweenPause;
struct TweenResume;
struct TweenYoyo;
struct TweenRepeat;

} // namespace event

class Tween : public Entity {
public:
	Tween() = default;
	explicit Tween(Entity entity);

	/// @param duration The time it takes to take progress from 0 to 1, or vice versa for reversed
	/// tweens. Yoyo tweens take twice the duration to complete a full
	/// yoyo cycle.
	Tween& During(milliseconds duration);

	template <typename T, typename... TArgs>
	Tween& AddScript(TArgs&&... args);

	template <typename T>
	using Callback = std::variant<std::function<void()>, std::function<void(T)>>;

	Tween& OnProgress(const Callback<event::TweenProgress>& callback);
	Tween& OnStart(const Callback<event::TweenStart>& callback);
	Tween& OnComplete(const Callback<event::TweenComplete>& callback);
	Tween& OnPointStart(const Callback<event::TweenPointStart>& callback);
	Tween& OnPointComplete(const Callback<event::TweenPointComplete>& callback);
	Tween& OnReset(const Callback<event::TweenReset>& callback);
	Tween& OnStop(const Callback<event::TweenStop>& callback);
	Tween& OnPause(const Callback<event::TweenPause>& callback);
	Tween& OnResume(const Callback<event::TweenResume>& callback);
	Tween& OnYoyo(const Callback<event::TweenYoyo>& callback);
	Tween& OnRepeat(const Callback<event::TweenRepeat>& callback);

	/// @return True if the tween has completed all of its tween points.
	[[nodiscard]] bool IsCompleted() const;

	/// @return True if the tween is started and not paused.
	[[nodiscard]] bool IsRunning() const;

	/// @return True if the tween has been started or is currently paused.
	[[nodiscard]] bool IsStarted() const;

	/// @return True if the tween is currently paused.
	[[nodiscard]] bool IsPaused() const;

	/// @brief Resets and starts the tween. Will restart paused tweens.
	/// @param force If true, ignores the current state of the tween. If false, will only start if
	/// the tween is paused or not currently started.
	Tween& Start(bool force = true);

	/// @brief Stops the tween.
	Tween& Stop();

	/// @brief Pause the tween.
	Tween& Pause();

	/// @brief Resume the tween.
	Tween& Resume();

	/// @brief Toggles the tween between paused and resumed, or if starts the tween if it is
	/// stopped.
	Tween& Toggle();

	/// @brief Will trigger OnReset callback for each tween point if the tween was started or
	/// completed.
	Tween& Reset();

	/// @brief Clears previously assigned tween points and resets the tween. Will skip invoking
	/// callbacks.
	Tween& Clear();

	Tween& Ease(ptgn::Ease ease);

	/// @brief nullopt for infinite repeats.
	Tween& Repeat(std::optional<std::size_t> repeats = std::nullopt);

	Tween& Reverse(bool reversed = true);

	Tween& Yoyo(bool yoyo = true);

	/// @brief Note: This value is impacted by the Ease value set for the current tween point.
	/// @return Current eased progress of the tween [0.0f, 1.0f].
	float GetProgress() const;

	/// @brief Note: This value is NOT impacted by the Ease value set for the current tween point.
	/// @return Current uneased progress of the tween [0.0f, 1.0f].
	float GetLinearProgress() const;

	/// @return Current number of repeats of the current tween point.
	std::size_t GetRepeats() const;

	/// @return The easing mode of the current tween point.
	ptgn::Ease GetEase() const;

	/// @param duration Duration to set for the current tween point.
	Tween& SetDuration(milliseconds duration);

	/// @return The duration of the current tween point.
	milliseconds GetDuration() const;

	// TODO: Implement and test.
	// dt in seconds.
	// float Rewind(secondsf dt) {
	// return Step(-dt);
	//}

	void Step(secondsf dt);

	/// @brief If there are future tween points, will simulate a tween point completion. If the
	/// tween has completed or is in the middle of the final tween point, this function does
	/// nothing.
	Tween& IncrementPoint();

	Tween& RemoveLastTweenPoint();

	void Seek(float new_progress);

	void Seek(milliseconds time);

	/// @return The number of valid tween points in the tween.
	std::size_t GetTweenPointCount() const;

	TweenPoint& GetLastTweenPoint();
	const TweenPoint& GetLastTweenPoint() const;

private:
	friend class Scene;
	friend class ScriptSequence;
	friend class SceneManager;

	/// @return Index of the current tween point, if a valid one exists.
	std::optional<std::size_t> GetCurrentIndex() const;

	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushEventToCurrentTweenPoint(TArgs&&... args);

	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushEventToAllTweenPoints(TArgs&&... args);

	milliseconds GetTotalDuration() const;

	const TweenPoint& GetCurrentTweenPoint() const;
	TweenPoint& GetCurrentTweenPoint();

	static void Update(Scene& scene, secondsf dt);
};

namespace impl {

template <typename T>
struct TweenEventBase : public Event<T> {
	TweenEventBase() = default;

	TweenEventBase(Tween tween, Entity parent) : tween{ tween }, parent{ parent } {}

	/// @brief Tween associated with the event.
	Tween tween;

	/// @brief Parent entity of the tween, or the tween itself if it has no parent.
	Entity parent;
};

} // namespace impl

namespace event {

struct TweenProgress : public impl::TweenEventBase<TweenProgress> {
	TweenProgress() = default;

	TweenProgress(Tween tween, Entity parent, float progress) :
		TweenEventBase{ tween, parent }, progress{ progress } {}

	operator float() const { // NOSONAR
		return progress;
	}

	/// @brief Value between [0.0f, 1.0f] indicating how much of the total duration the tween has
	/// passed in the current repetition. Note: This value remains 0.0f to 1.0f even when the tween
	/// is reversed or yoyoing.
	float progress{ 0.0f };
};

struct TweenComplete : public impl::TweenEventBase<TweenComplete> {
	using TweenEventBase::TweenEventBase;
};

struct TweenPointStart : public impl::TweenEventBase<TweenPointStart> {
	using TweenEventBase::TweenEventBase;
};

struct TweenPointComplete : public impl::TweenEventBase<TweenPointComplete> {
	using TweenEventBase::TweenEventBase;
};

struct TweenReset : public impl::TweenEventBase<TweenReset> {
	using TweenEventBase::TweenEventBase;
};

struct TweenStart : public impl::TweenEventBase<TweenStart> {
	using TweenEventBase::TweenEventBase;
};

struct TweenStop : public impl::TweenEventBase<TweenStop> {
	using TweenEventBase::TweenEventBase;
};

struct TweenPause : public impl::TweenEventBase<TweenPause> {
	using TweenEventBase::TweenEventBase;
};

struct TweenResume : public impl::TweenEventBase<TweenResume> {
	using TweenEventBase::TweenEventBase;
};

struct TweenYoyo : public impl::TweenEventBase<TweenYoyo> {
	using TweenEventBase::TweenEventBase;
};

struct TweenRepeat : public impl::TweenEventBase<TweenRepeat> {
	using TweenEventBase::TweenEventBase;
};

} // namespace event

class TweenPoint {
public:
	TweenPoint()		   = default;
	~TweenPoint() noexcept = default;

	TweenPoint(const TweenPoint&)			 = delete;
	TweenPoint& operator=(const TweenPoint&) = delete;

	TweenPoint(TweenPoint&&) noexcept			 = default;
	TweenPoint& operator=(TweenPoint&&) noexcept = default;

	bool operator==(const TweenPoint&) const = default;

	/// @return True if the tween point has infinite repeats.
	[[nodiscard]] bool IsInfinite() const;

	/// @return True if the tween point has a duration of 0, meaning it will complete instantly.
	[[nodiscard]] bool IsInstant() const;

private:
	friend class Tween;
	friend class impl::TweenData;
	friend class ScriptSequence;

	/// @brief Current number of repetitions of the tween.
	std::size_t current_repeat_{ 0 };

	/// @brief Total number of repetitions of the tween (nullopt for infinite tween).
	std::optional<std::size_t> total_repeats_{ 0 };

	/// @brief Go back and fourth between values (requires repeat != 0) (both
	/// directions take duration time).
	bool yoyo_{ false };

	bool currently_reversed_{ false };

	bool start_reversed_{ false };

	milliseconds duration_{ 0 };

	/// @brief Easing function between tween start and end value.
	Ease ease_{ Ease::Linear };

	bool flagged_for_removal_{ false };

	impl::Scripts script_container_;

	LocalEventHandler events_;

	// TODO: Fix serialization.
	// PTGN_SERIALIZER_REGISTER_NAMED(
	//	TweenPoint, KeyValue("current_repeat", current_repeat_),
	//	KeyValue("total_repeats", total_repeats_), KeyValue("yoyo", yoyo_),
	//	KeyValue("currently_reversed", currently_reversed_),
	//	KeyValue("start_reversed", start_reversed_), KeyValue("duration", duration_),
	//	KeyValue("ease", ease_), KeyValue("script_container", script_container_)
	//)
};

namespace impl {

enum class TweenState {
	Stopped,
	Started,
	Paused,
	Completed
};

PTGN_SERIALIZE_ENUM(
	TweenState, { { TweenState::Stopped, "stopped" },
				  { TweenState::Started, "started" },
				  { TweenState::Paused, "paused" },
				  { TweenState::Completed, "completed" } }
);

class TweenData {
public:
	TweenData()			  = default;
	~TweenData() noexcept = default;

	TweenData(const TweenData&)			   = delete;
	TweenData& operator=(const TweenData&) = delete;

	TweenData(TweenData&&) noexcept			   = default;
	TweenData& operator=(TweenData&&) noexcept = default;

	/// @brief Value between [0.0f, 1.0f] indicating how much of the total duration the tween has
	/// passed in the current repetition. Note: This value remains 0.0f to 1.0f even when the tween
	/// is reversed or yoyoing.
	float progress_{ 0.0f };

	TweenState state_{ TweenState::Stopped };

	// TODO: Fix serialization.
	// PTGN_SERIALIZER_REGISTER_NAMED(
	//	TweenData, KeyValue("progress", progress_), KeyValue("index", index_),
	//	KeyValue("points", points_), KeyValue("state", state_)
	//)

	/// @return Index of the current tween point if there is a valid current tween point,
	/// std::nullopt otherwise.
	std::optional<std::size_t> GetCurrentIndex() const;

	/// @return Index of the last tween point if there are any valid tween points available,
	/// std::nullopt otherwise.
	std::optional<std::size_t> GetLastIndex() const;

	void IncrementIndex();

	void RemoveLastTweenPoint();

	void Clear() const;

	void Reset();

	/// @return True if there is a valid tween point following the current one, false if there are
	/// no more valid tween points, or std::nullopt if there are no valid tween points at all.
	[[nodiscard]] std::optional<bool> FutureTweenPointIsValid() const;

	std::size_t GetValidPointCount() const;

	[[nodiscard]] bool IsEmpty() const;

	TweenPoint& EmplaceTweenPoint();

	milliseconds GetTotalDuration() const;

	const TweenPoint& GetCurrentTweenPoint() const;
	TweenPoint& GetCurrentTweenPoint();

	const TweenPoint& GetLastTweenPoint() const;
	TweenPoint& GetLastTweenPoint();

	void OnEvent();

	void ApplyPending() const;

	void ClearFlagged();

	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushEventToAllTweenPoints(TArgs&&... args);

	/// @brief Does nothing if there is no valid current tween point.
	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushEventToCurrentTweenPoint(TArgs&&... args);

private:
	std::vector<std::unique_ptr<TweenPoint>> points_;

	/// @brief Not a reliable indicator of what is the current tween point as tween points may be
	/// flagged for removal, which makes them invalid. This index is updates every time the user
	/// requests it.
	mutable std::size_t index_{ 0 };
};

template <typename T>
struct TweenScript : public Script {
	TweenScript() = default;

	explicit TweenScript(const Tween::Callback<T>& callback) : callback_{ callback } {}

	void OnEvent(EventDispatcher dispatcher) override {
		dispatcher.DispatchVariant<T>(callback_);
	}

private:
	Tween::Callback<T> callback_;
};

using TweenStartScript		   = TweenScript<ptgn::event::TweenStart>;
using TweenCompleteScript	   = TweenScript<ptgn::event::TweenComplete>;
using TweenPointStartScript	   = TweenScript<ptgn::event::TweenPointStart>;
using TweenPointCompleteScript = TweenScript<ptgn::event::TweenPointComplete>;
using TweenResetScript		   = TweenScript<ptgn::event::TweenReset>;
using TweenStopScript		   = TweenScript<ptgn::event::TweenStop>;
using TweenPauseScript		   = TweenScript<ptgn::event::TweenPause>;
using TweenResumeScript		   = TweenScript<ptgn::event::TweenResume>;
using TweenYoyoScript		   = TweenScript<ptgn::event::TweenYoyo>;
using TweenRepeatScript		   = TweenScript<ptgn::event::TweenRepeat>;
using TweenProgressScript	   = TweenScript<ptgn::event::TweenProgress>;

template <EventType T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
void TweenData::PushEventToAllTweenPoints(TArgs&&... args) {
	for (const auto& point : points_) {
		PTGN_ASSERT(point);
		if (point->flagged_for_removal_) {
			continue;
		}
		point->events_.Push<T>(std::nullopt, std::forward<TArgs>(args)...);
	}
}

template <EventType T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
void TweenData::PushEventToCurrentTweenPoint(TArgs&&... args) {
	auto current_index{ GetCurrentIndex() };
	if (!current_index.has_value() || *current_index >= points_.size() ||
		!points_[*current_index]) {
		return;
	}
	points_[*current_index]->events_.Push<T>(std::nullopt, std::forward<TArgs>(args)...);
}

} // namespace impl

std::ostream& operator<<(std::ostream& os, impl::TweenState state);

template <EventType T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
void Tween::PushEventToCurrentTweenPoint(TArgs&&... args) {
	auto& tween{ Get<impl::TweenData>() };
	tween.PushEventToCurrentTweenPoint<T>(std::forward<TArgs>(args)...);
}

template <EventType T, typename... TArgs>
	requires std::constructible_from<T, TArgs...>
void Tween::PushEventToAllTweenPoints(TArgs&&... args) {
	auto& tween{ Get<impl::TweenData>() };
	tween.PushEventToAllTweenPoints<T>(std::forward<TArgs>(args)...);
}

template <typename T, typename... TArgs>
Tween& Tween::AddScript(TArgs&&... args) {
	auto& container{ GetLastTweenPoint().script_container_ };
	container.Add<T>(*this, std::forward<TArgs>(args)...);
	return *this;
}

Tween CreateTween(Scene& scene);

} // namespace ptgn