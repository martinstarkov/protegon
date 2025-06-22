#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/entity.h"
#include "core/script.h"
#include "core/time.h"
#include "math/easing.h"
#include "math/math.h"
#include "serialization/serializable.h"

namespace ptgn {

class Manager;
class Scene;

namespace impl {

struct TweenPoint;

} // namespace impl

class Tween : public Entity {
public:
	Tween() = default;
	Tween(const Entity& entity);

	// @param duration The time it takes to take progress from 0 to 1, or vice versa for reversed
	// tweens. Yoyo tweens take twice the duration to complete a full
	// yoyo cycle.
	Tween& During(milliseconds duration);
	Tween& Ease(const ptgn::Ease& ease);

	// -1 for infinite repeats.
	Tween& Repeat(std::int64_t repeats);
	Tween& Reverse(bool reversed = true);
	Tween& Yoyo(bool yoyo = true);

	template <typename T, typename... TArgs>
	Tween& AddTweenScript(TArgs&&... args);

	// @return Current progress of the tween [0.0f, 1.0f].
	[[nodiscard]] float GetProgress() const;

	// @return Current number of repeats of the current tween point.
	[[nodiscard]] std::int64_t GetRepeats() const;

	// @return True if the tween is started and not paused.
	[[nodiscard]] bool IsRunning() const;

	// @return True if the tween has completed all of its tween points.
	[[nodiscard]] bool IsCompleted() const;

	// @return True if the tween has been started or is currently paused.
	[[nodiscard]] bool IsStarted() const;

	// @return True if the tween is currently paused.
	[[nodiscard]] bool IsPaused() const;

	// TODO: Implement and test.
	// dt in seconds.
	// float Rewind(float dt) {
	// return Step(-dt);
	//}

	// dt in seconds.
	// @return New progress of the tween after stepping.
	float Step(float dt);

	// @return New progress of the tween after seeking.
	float Seek(float new_progress);

	// @return New progress of the tween after seeking.
	float Seek(milliseconds time);

	// Resets and starts the tween. Will restart paused tweens.
	// @param force If true, ignores the current state of the tween. If false, will only start if
	// the tween is paused or not currently started.
	Tween& Start(bool force = true);

	// If there are future tween points, will simulate a tween point completion. If the tween has
	// completed or is in the middle of the final tween point, this function does nothing.
	Tween& IncrementTweenPoint();

	// @return Index of the current tween point.
	[[nodiscard]] std::size_t GetCurrentIndex() const;

	// Toggles the tween between started and stopped.
	Tween& Toggle();

	// Pause the tween.
	Tween& Pause();

	// Resume the tween.
	Tween& Resume();

	// Will trigger OnStop callback if tween was started or completed.
	Tween& Reset();

	// Stops the tween.
	Tween& Stop();

	// Clears previously assigned tween points and resets the tween.
	Tween& Clear();

	// @param tween_point_index Which tween point to query to duration of.
	// @return The duration of the specified tween point.
	[[nodiscard]] milliseconds GetDuration(std::size_t tween_point_index = 0) const;

	// @param duration Duration to set for the tween.
	// @param tween_point_index Which tween point to set the duration of.
	Tween& SetDuration(milliseconds duration, std::size_t tween_point_index = 0);

private:
	// @return New progress of the tween after seeking.
	[[nodiscard]] float SeekImpl(float new_progress);

	// @return New progress of the tween after stepping.
	[[nodiscard]] float StepImpl(float dt, bool accumulate_progress);

	// @return New progress of the tween after accumulating.
	[[nodiscard]] float AccumulateProgress(float new_progress);

	void PointCompleted();
	void HandleCallbacks(bool suppress_update);

	// @return New progress of the tween after updating.
	float UpdateImpl(bool suppress_update = false);

	[[nodiscard]] float GetNewProgress(duration<float> time) const;

	[[nodiscard]] impl::TweenPoint& GetCurrentTweenPoint();
	[[nodiscard]] const impl::TweenPoint& GetCurrentTweenPoint() const;
	[[nodiscard]] impl::TweenPoint& GetLastTweenPoint();
};

struct TweenInfo {
	TweenInfo() = delete;

	TweenInfo(const Tween& tween_object, float tween_progress, const Entity& tween_parent) :
		tween{ tween_object }, progress{ tween_progress }, parent{ tween_parent } {}

	Tween tween;
	float progress{ 0.0f };
	Entity parent; // can be same as "tween" if tween is not attached to another entity.
};

namespace impl {

class ITweenScript {
public:
	virtual ~ITweenScript() = default;

	virtual void OnComplete([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnRepeat([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnYoyo([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnStart([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnStop([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnUpdate([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnPause([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnResume([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual void OnReset([[maybe_unused]] TweenInfo info) { /* user implementation */ }

	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;
};

struct TweenPoint {
	TweenPoint() = default;

	explicit TweenPoint(milliseconds duration);

	void SetReversed(bool reversed);

	// current number of repetitions of the tween.
	std::int64_t current_repeat_{ 0 };

	// total number of repetitions of the tween (-1 for infinite tween).
	std::int64_t total_repeats_{ 0 };

	// go back and fourth between values (requires repeat != 0) (both
	// directions take duration time).
	bool yoyo_{ false };

	bool currently_reversed_{ false };

	bool start_reversed_{ false };

	milliseconds duration_{ 0 };

	// easing function between tween start and end value.
	Ease ease_{ SymmetricalEase::Linear };

	ScriptContainer<ITweenScript> script_container_;

	PTGN_SERIALIZER_REGISTER_NAMED(
		TweenPoint, KeyValue("current_repeat", current_repeat_),
		KeyValue("total_repeats", total_repeats_), KeyValue("yoyo", yoyo_),
		KeyValue("currently_reversed", currently_reversed_),
		KeyValue("start_reversed", start_reversed_), KeyValue("duration", duration_),
		KeyValue("ease", ease_), KeyValue("script_container", script_container_)
	)
};

struct TweenInstance {
	TweenInstance()								   = default;
	TweenInstance(const TweenInstance&)			   = default;
	TweenInstance& operator=(const TweenInstance&) = default;
	TweenInstance(TweenInstance&&) noexcept;
	TweenInstance& operator=(TweenInstance&&) noexcept;
	~TweenInstance();

	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	std::size_t index_{ 0 };
	std::vector<TweenPoint> points_;

	bool paused_{ false };
	bool started_{ false };

	PTGN_SERIALIZER_REGISTER_NAMED(
		TweenInstance, KeyValue("progress", progress_), KeyValue("index", index_),
		KeyValue("tween_points", points_), KeyValue("paused", paused_),
		KeyValue("started", started_)
	)
};

} // namespace impl

template <typename T, typename... TArgs>
Tween& Tween::AddTweenScript(TArgs&&... args) {
	GetLastTweenPoint().script_container_.AddScript<T>(std::forward<TArgs>(args)...);
	return *this;
}

template <typename T>
using TweenScript = impl::Script<T, impl::ITweenScript>;

Tween CreateTween(Scene& scene);

} // namespace ptgn