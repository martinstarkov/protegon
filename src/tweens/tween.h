#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "core/entity.h"
#include "core/script.h"
#include "core/time.h"
#include "math/easing.h"
#include "serialization/serializable.h"

namespace ptgn {

class Manager;
class Scene;

namespace impl {

struct TweenPoint;

} // namespace impl

using TweenCallback = std::function<void(Entity)>;

class Tween : public Entity {
public:
	Tween() = default;
	Tween(const Entity& entity);

	// @param duration The time it takes to take progress from 0 to 1, or vice versa for reversed
	// tweens. Yoyo tweens take twice the duration to complete a full
	// yoyo cycle.
	Tween& During(milliseconds duration);

	template <typename T, typename... TArgs>
	Tween& AddScript(TArgs&&... args);

	// TODO: Add variant functions with no entity argument.

	Tween& OnProgress(const std::function<void(Entity, float)>& func);
	Tween& OnStart(const TweenCallback& func);
	Tween& OnComplete(const TweenCallback& func);
	Tween& OnPointStart(const TweenCallback& func);
	Tween& OnPointComplete(const TweenCallback& func);
	Tween& OnReset(const TweenCallback& func);
	Tween& OnStop(const TweenCallback& func);
	Tween& OnPause(const TweenCallback& func);
	Tween& OnResume(const TweenCallback& func);
	Tween& OnYoyo(const TweenCallback& func);
	Tween& OnRepeat(const TweenCallback& func);

	// @return True if the tween has completed all of its tween points.
	[[nodiscard]] bool IsCompleted() const;

	// @return True if the tween is started and not paused.
	[[nodiscard]] bool IsRunning() const;

	// @return True if the tween has been started or is currently paused.
	[[nodiscard]] bool IsStarted() const;

	// @return True if the tween is currently paused.
	[[nodiscard]] bool IsPaused() const;

	// Resets and starts the tween. Will restart paused tweens.
	// @param force If true, ignores the current state of the tween. If false, will only start if
	// the tween is paused or not currently started.
	Tween& Start(bool force = true);

	// Stops the tween.
	Tween& Stop();

	// Pause the tween.
	Tween& Pause();

	// Resume the tween.
	Tween& Resume();

	// Toggles the tween between paused and resumed, or if starts the tween if it is stopped.
	Tween& Toggle();

	// Will trigger OnReset callback for each tween point if the tween was started or completed.
	Tween& Reset();

	// Clears previously assigned tween points and resets the tween. Will skip invoking callbacks.
	Tween& Clear();

	Tween& Ease(const ptgn::Ease& ease);

	// -1 for infinite repeats.
	Tween& Repeat(std::int64_t repeats);

	Tween& Reverse(bool reversed = true);

	Tween& Yoyo(bool yoyo = true);

	// Note: This value is impacted by the Ease value set for the current tween point.
	// @return Current progress of the tween [0.0f, 1.0f].
	[[nodiscard]] float GetProgress() const;

	// @return Current number of repeats of the current tween point.
	[[nodiscard]] std::int64_t GetRepeats() const;

	// @return Index of the current tween point.
	[[nodiscard]] std::size_t GetCurrentIndex() const;

	// @param duration Duration to set for the tween.
	// @param tween_point_index Which tween point to set the duration of.
	Tween& SetDuration(milliseconds duration, std::size_t tween_point_index);

	// @param tween_point_index Which tween point to query to duration of.
	// @return The duration of the specified tween point.
	[[nodiscard]] milliseconds GetDuration(std::size_t tween_point_index = 0) const;

	// TODO: Implement and test.
	// dt in seconds.
	// float Rewind(float dt) {
	// return Step(-dt);
	//}

	// dt in seconds.
	void Step(float dt);

	// If there are future tween points, will simulate a tween point completion. If the tween has
	// completed or is in the middle of the final tween point, this function does nothing.
	Tween& IncrementPoint();

	void Seek(float new_progress);

	void Seek(milliseconds time);

private:
	friend class Scene;

	[[nodiscard]] milliseconds GetTotalDuration() const;

	[[nodiscard]] const impl::TweenPoint& GetCurrentTweenPoint() const;
	[[nodiscard]] impl::TweenPoint& GetCurrentTweenPoint();
	[[nodiscard]] impl::TweenPoint& GetLastTweenPoint();

	static void Update(Scene& scene, float dt);
};

namespace impl {

struct TweenPoint {
	bool operator==(const TweenPoint&) const = default;

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

	Scripts script_container_;

	PTGN_SERIALIZER_REGISTER_NAMED(
		TweenPoint, KeyValue("current_repeat", current_repeat_),
		KeyValue("total_repeats", total_repeats_), KeyValue("yoyo", yoyo_),
		KeyValue("currently_reversed", currently_reversed_),
		KeyValue("start_reversed", start_reversed_), KeyValue("duration", duration_),
		KeyValue("ease", ease_), KeyValue("script_container", script_container_)
	)
};

enum class TweenState {
	Stopped,
	Started,
	Paused,
	Completed
};

struct TweenInstance {
	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	std::size_t index_{ 0 };
	std::vector<TweenPoint> points_;

	TweenState state_{ TweenState::Stopped };

	PTGN_SERIALIZER_REGISTER_NAMED(
		TweenInstance, KeyValue("progress", progress_), KeyValue("index", index_),
		KeyValue("points", points_), KeyValue("state", state_)
	)
};

template <typename T, typename... Ts>
class GenericTweenScript : public Script<T, TweenScript> {
public:
	GenericTweenScript() = default;

	explicit GenericTweenScript(const std::function<void(Entity, Ts...)>& callback) :
		callback_{ callback } {}

	void TryInvoke(Entity entity, Ts... args) {
		if (callback_) {
			callback_(entity, args...);
		}
	}

protected:
	std::function<void(Entity, Ts...)> callback_;
};

#define PTGN_DEFINE_TWEEN_SCRIPT(NAME, METHOD)              \
	struct NAME : public GenericTweenScript<NAME> {         \
		using GenericTweenScript<NAME>::GenericTweenScript; \
		void METHOD() override {                            \
			TryInvoke(entity);                              \
		}                                                   \
	};

#define PTGN_DEFINE_TWEEN_SCRIPT_WITH_ARGS(NAME, METHOD, ...)            \
	struct NAME : public GenericTweenScript<NAME, __VA_ARGS__> {         \
		using GenericTweenScript<NAME, __VA_ARGS__>::GenericTweenScript; \
		void METHOD(__VA_ARGS__ args) override {                         \
			TryInvoke(entity, args);                                     \
		}                                                                \
	};

PTGN_DEFINE_TWEEN_SCRIPT_WITH_ARGS(TweenProgressScript, OnProgress, float)
PTGN_DEFINE_TWEEN_SCRIPT(TweenCompleteScript, OnComplete)
PTGN_DEFINE_TWEEN_SCRIPT(TweenPointCompleteScript, OnPointComplete)
PTGN_DEFINE_TWEEN_SCRIPT(TweenResetScript, OnReset)
PTGN_DEFINE_TWEEN_SCRIPT(TweenPointStartScript, OnPointStart)
PTGN_DEFINE_TWEEN_SCRIPT(TweenStartScript, OnStart)
PTGN_DEFINE_TWEEN_SCRIPT(TweenStopScript, OnStop)
PTGN_DEFINE_TWEEN_SCRIPT(TweenPauseScript, OnPause)
PTGN_DEFINE_TWEEN_SCRIPT(TweenResumeScript, OnResume)
PTGN_DEFINE_TWEEN_SCRIPT(TweenYoyoScript, OnYoyo)
PTGN_DEFINE_TWEEN_SCRIPT(TweenRepeatScript, OnRepeat)

PTGN_SERIALIZER_REGISTER_ENUM(
	TweenState, { { TweenState::Stopped, "stopped" },
				  { TweenState::Started, "started" },
				  { TweenState::Paused, "paused" },
				  { TweenState::Completed, "completed" } }
);

} // namespace impl

template <typename T, typename... TArgs>
Tween& Tween::AddScript(TArgs&&... args) {
	auto& script{ GetLastTweenPoint().script_container_.AddScript<T>(std::forward<TArgs>(args)...
	) };
	script.entity = *this;
	return *this;
}

Tween CreateTween(Scene& scene);

} // namespace ptgn