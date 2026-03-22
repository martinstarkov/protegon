#pragma once

#include <cstdint>
#include <functional>
#include <ostream>
#include <vector>

#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/math/easing.h"
#include "core/time/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

class Scene;

namespace impl {

struct TweenPoint;

} // namespace impl

using TweenCallback = std::function<void(Entity)>;

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

	/// @brief -1 for infinite repeats.
	Tween& Repeat(std::int64_t repeats);

	Tween& Reverse(bool reversed = true);

	Tween& Yoyo(bool yoyo = true);

	/// @brief Note: This value is impacted by the Ease value set for the current tween point.
	/// @return Current eased progress of the tween [0.0f, 1.0f].
	float GetProgress() const;

	/// @brief Note: This value is NOT impacted by the Ease value set for the current tween point.
	/// @return Current uneased progress of the tween [0.0f, 1.0f].
	float GetLinearProgress() const;

	/// @return Current number of repeats of the current tween point.
	std::int64_t GetRepeats() const;

	/// @return Index of the current tween point.
	std::size_t GetCurrentIndex() const;

	/// @return The easing mode of the current tween point.
	ptgn::Ease GetEase() const;

	/// @param duration Duration to set for the tween.
	/// @param tween_point_index Which tween point to set the duration of.
	Tween& SetDuration(milliseconds duration, std::size_t tween_point_index);

	/// @param tween_point_index Which tween point to query to duration of.
	/// @return The duration of the specified tween point.
	milliseconds GetDuration(std::size_t tween_point_index = 0) const;

	// TODO: Implement and test.
	// dt in seconds.
	// float Rewind(float dt) {
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

	std::size_t GetTweenPointCount() const;

	const impl::TweenPoint& GetTweenPoint(std::size_t tween_point_index) const;
	impl::TweenPoint& GetTweenPoint(std::size_t tween_point_index);

	impl::TweenPoint& GetLastTweenPoint();
	const impl::TweenPoint& GetLastTweenPoint() const;

private:
	friend class Scene;
	friend class ScriptSequence;
	friend class SceneManager;

	milliseconds GetTotalDuration() const;

	const impl::TweenPoint& GetCurrentTweenPoint() const;
	impl::TweenPoint& GetCurrentTweenPoint();

	static void Update(Scene& scene, secondsf dt);
};

struct TweenProgress : public Event<TweenProgress> {
	TweenProgress() = default;

	explicit TweenProgress(float progress) : progress{ progress } {}

	/// @brief Value between [0.0f, 1.0f] indicating how much of the total duration the tween has
	/// passed in the current repetition. Note: This value remains 0.0f to 1.0f even when the tween
	/// is reversed or yoyoing.
	float progress{ 0.0f };
};

struct TweenComplete : public Event<TweenComplete> {};

struct TweenPointStart : public Event<TweenPointStart> {};

struct TweenPointComplete : public Event<TweenPointComplete> {};

struct TweenReset : public Event<TweenReset> {};

struct TweenStart : public Event<TweenStart> {};

struct TweenStop : public Event<TweenStop> {};

struct TweenPause : public Event<TweenPause> {};

struct TweenResume : public Event<TweenResume> {};

struct TweenYoyo : public Event<TweenYoyo> {};

struct TweenRepeat : public Event<TweenRepeat> {};

namespace impl {

struct TweenPoint {
	TweenPoint()		   = default;
	~TweenPoint() noexcept = default;

	TweenPoint(const TweenPoint&)			 = delete;
	TweenPoint& operator=(const TweenPoint&) = delete;

	TweenPoint(TweenPoint&&) noexcept			 = default;
	TweenPoint& operator=(TweenPoint&&) noexcept = default;

	bool operator==(const TweenPoint&) const = default;

	/// @brief Current number of repetitions of the tween.
	std::int64_t current_repeat_{ 0 };

	/// @brief Total number of repetitions of the tween (-1 for infinite tween).
	std::int64_t total_repeats_{ 0 };

	/// @brief Go back and fourth between values (requires repeat != 0) (both
	/// directions take duration time).
	bool yoyo_{ false };

	bool currently_reversed_{ false };

	bool start_reversed_{ false };

	milliseconds duration_{ 0 };

	/// @brief Easing function between tween start and end value.
	Ease ease_{ Ease::Linear };

	Scripts script_container_;

	// TODO: Fix serialization.
	// PTGN_SERIALIZER_REGISTER_NAMED(
	//	TweenPoint, KeyValue("current_repeat", current_repeat_),
	//	KeyValue("total_repeats", total_repeats_), KeyValue("yoyo", yoyo_),
	//	KeyValue("currently_reversed", currently_reversed_),
	//	KeyValue("start_reversed", start_reversed_), KeyValue("duration", duration_),
	//	KeyValue("ease", ease_), KeyValue("script_container", script_container_)
	//)
};

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

struct TweenInstance {
	TweenInstance()			  = default;
	~TweenInstance() noexcept = default;

	TweenInstance(const TweenInstance&)			   = delete;
	TweenInstance& operator=(const TweenInstance&) = delete;

	TweenInstance(TweenInstance&&) noexcept			   = default;
	TweenInstance& operator=(TweenInstance&&) noexcept = default;

	/// @brief Value between [0.0f, 1.0f] indicating how much of the total duration the tween has
	/// passed in the current repetition. Note: This value remains 0.0f to 1.0f even when the tween
	/// is reversed or yoyoing.
	float progress_{ 0.0f };

	std::size_t index_{ 0 };
	std::vector<TweenPoint> points_;

	TweenState state_{ TweenState::Stopped };

	// TODO: Fix serialization.
	// PTGN_SERIALIZER_REGISTER_NAMED(
	//	TweenInstance, KeyValue("progress", progress_), KeyValue("index", index_),
	//	KeyValue("points", points_), KeyValue("state", state_)
	//)
};

template <typename T>
struct TweenCallbackScript : public Script {
	TweenCallbackScript() = default;

	explicit TweenCallbackScript(const TweenCallback& callback) : callback_{ callback } {}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<T>([this](const T&) { std::invoke(callback_, entity); });
	}

private:
	TweenCallback callback_;
};

using TweenStartScript		   = TweenCallbackScript<TweenStart>;
using TweenCompleteScript	   = TweenCallbackScript<TweenComplete>;
using TweenPointStartScript	   = TweenCallbackScript<TweenPointStart>;
using TweenPointCompleteScript = TweenCallbackScript<TweenPointComplete>;
using TweenResetScript		   = TweenCallbackScript<TweenReset>;
using TweenStopScript		   = TweenCallbackScript<TweenStop>;
using TweenPauseScript		   = TweenCallbackScript<TweenPause>;
using TweenResumeScript		   = TweenCallbackScript<TweenResume>;
using TweenYoyoScript		   = TweenCallbackScript<TweenYoyo>;
using TweenRepeatScript		   = TweenCallbackScript<TweenRepeat>;

struct TweenProgressScript : public Script {
	TweenProgressScript() = default;

	explicit TweenProgressScript(const std::function<void(Entity, float)>& callback) :
		callback_{ callback } {}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<TweenProgress>([this](const TweenProgress& e) {
			std::invoke(callback_, entity, e.progress);
		});
	}

private:
	std::function<void(Entity, float)> callback_;
};

} // namespace impl

std::ostream& operator<<(std::ostream& os, impl::TweenState state);

template <typename T, typename... TArgs>
Tween& Tween::AddScript(TArgs&&... args) {
	auto& script{
		GetLastTweenPoint().script_container_.Add<T>(*this, std::forward<TArgs>(args)...)
	};
	return *this;
}

Tween CreateTween(Scene& scene);

} // namespace ptgn