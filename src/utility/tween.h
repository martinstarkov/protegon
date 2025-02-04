#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/manager.h"
#include "math/math.h"
#include "utility/assert.h"
#include "utility/handle.h"
#include "utility/time.h"
#include "utility/type_traits.h"

namespace ptgn {

enum class TweenEase {
	Linear,
	InSine,
	OutSine,
	InOutSine,
	/*InQuad,
	OutQuad,
	InOutQuad,
	InCubic,
	OutCubic,
	InOutCubic,
	InExponential,
	OutExponential,
	InOutExponential,
	InCircular,
	OutCircular,
	InOutCircular,*/
	// TODO: Implement Custom ease
};

class Tween;

using TweenCallback = std::variant<
	std::function<void(Tween&, float)>, std::function<void(Tween&)>, std::function<void(float)>,
	std::function<void()>>;

namespace impl {

class Game;
class TweenManager;

using TweenEaseFunction = std::function<float(float, float, float)>;

const static std::unordered_map<TweenEase, TweenEaseFunction> tween_ease_functions_{
	{ TweenEase::Linear,
	  [](float t, float a, float b) {
		  float c{ b - a };
		  return a + t * c;
	  } },
	{ TweenEase::InSine,
	  [](float t, float a, float b) {
		  float c{ b - a };
		  return -c * std::cos(t * half_pi<float>) + b;
	  } },
	{ TweenEase::OutSine,
	  [](float t, float a, float b) {
		  float c{ b - a };
		  return c * std::sin(t * half_pi<float>) + a;
	  } },
	{ TweenEase::InOutSine,
	  [](float t, float a, float b) {
		  float c{ b - a };
		  return -c / float{ 2 } * (std::cos(pi<float> * t) - float{ 1 }) + a;
	  } },
	// TODO: Implement these.
	//{ TweenEase::InQuad,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::OutQuad,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InOutQuad,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InCubic,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::OutCubic,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InOutCubic,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InExponential,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::OutExponential,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InOutExponential,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InCircular,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::OutCircular,
	//  [](float t, float a, float b) {
	//  } },
	//{ TweenEase::InOutCircular,
	//  [](float t, float a, float b) {
	//  } },
};

struct TweenPoint {
	explicit TweenPoint(milliseconds duration) : duration_{ duration } {}

	void SetReversed(bool reversed) {
		start_reversed_		= reversed;
		currently_reversed_ = start_reversed_;
	}

	milliseconds duration_{ 0 };

	TweenEaseFunction easing_func_{
		tween_ease_functions_.find(TweenEase::Linear)->second
	};								   // easing function between tween start and end value.

	std::int64_t current_repeat_{ 0 }; // current number of repetitions of the tween.

	std::int64_t total_repeats_{
		0
	};					 // total number of repetitions of the tween (-1 for infinite tween).

	bool yoyo_{ false }; // go back and fourth between values (requires repeat != 0) (both
						 // directions take duration time).
	bool currently_reversed_{ false };
	bool start_reversed_{ false };

	TweenCallback on_complete_;
	TweenCallback on_repeat_;
	TweenCallback on_yoyo_;
	TweenCallback on_start_;
	TweenCallback on_stop_;
	TweenCallback on_update_;
	TweenCallback on_pause_;
	TweenCallback on_resume_;
};

struct TweenInstance {
	TweenInstance()								   = default;
	TweenInstance(const TweenInstance&)			   = default;
	TweenInstance(TweenInstance&&)				   = default;
	TweenInstance& operator=(const TweenInstance&) = default;
	TweenInstance& operator=(TweenInstance&&)	   = default;
	~TweenInstance();

	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	std::size_t index_{ 0 };
	std::vector<TweenPoint> tweens_points_;

	TweenCallback on_reset_;

	bool paused_{ false };
	bool started_{ false };

	[[nodiscard]] bool IsCompleted() const;

	[[nodiscard]] float GetNewProgress(duration<float> time) const;
	[[nodiscard]] float GetProgress() const;

	[[nodiscard]] TweenPoint& GetCurrentTweenPoint();
	[[nodiscard]] const TweenPoint& GetCurrentTweenPoint() const;
	[[nodiscard]] TweenPoint& GetLastTweenPoint();
};

} // namespace impl

class Tween : public Handle<impl::TweenInstance> {
public:
	Tween() = default;

	Tween& During(milliseconds duration);
	Tween& Ease(TweenEase ease);

	// -1 for infinite repeats.
	Tween& Repeat(std::int64_t repeats);
	Tween& Reverse(bool reversed = true);
	Tween& Yoyo(bool yoyo = true);

	Tween& OnUpdate(const TweenCallback& callback);
	Tween& OnStart(const TweenCallback& callback);
	Tween& OnComplete(const TweenCallback& callback);
	Tween& OnStop(const TweenCallback& callback);
	Tween& OnPause(const TweenCallback& callback);
	Tween& OnResume(const TweenCallback& callback);
	Tween& OnRepeat(const TweenCallback& callback);
	Tween& OnYoyo(const TweenCallback& callback);
	Tween& OnReset(const TweenCallback& callback);

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
	Tween& Start();

	// Starts the tween only if it is not already running.
	Tween& StartIfNotRunning();

	// If there are future tween points, will simulate a tween point completion. If the tween has
	// completed or is in the middle of the final tween point, this function does nothing.
	Tween& IncrementTweenPoint();

	// Pause the tween.
	Tween& Pause();

	// Resume the tween.
	Tween& Resume();

	// Will trigger OnStop callback if tween was started or completed.
	Tween& Reset();

	// Stops the tween.
	Tween& Stop();

	// Clears previously assigned tween points.
	Tween& Clear();

	// @param tween_point_index Which tween point to query to duration of.
	// @return The duration of the specified tween point.
	template <typename Duration = milliseconds>
	[[nodiscard]] Duration GetDuration(std::size_t tween_point_index = 0) const {
		const auto& t{ Get() };
		PTGN_ASSERT(
			tween_point_index < t.tweens_points_.size(),
			"Specified tween point index is out of range. Ensure tween points has been added "
			"beforehand"
		);
		return std::chrono::duration_cast<Duration>(t.tweens_points_[tween_point_index].duration_);
	}

	// @param duration Duration to set for the tween.
	// @param tween_point_index Which tween point to set the duration of.
	Tween& SetDuration(milliseconds duration, std::size_t tween_point_index = 0);

private:
	friend class impl::TweenManager;

	// @return New progress of the tween after seeking.
	[[nodiscard]] float SeekImpl(float new_progress);
	// @return New progress of the tween after stepping.
	[[nodiscard]] float StepImpl(float dt, bool accumulate_progress);
	// @return New progress of the tween after accumulating.
	[[nodiscard]] float AccumulateProgress(float new_progress);

	void ActivateCallback(const TweenCallback& callback);
	void PointCompleted();
	void HandleCallbacks(bool suppress_update);

	// @return New progress of the tween after updating.
	float UpdateImpl(bool suppress_update = false);
};

namespace impl {

class TweenManager : public MapManagerWithNameless<Tween> {
public:
	TweenManager()									 = default;
	~TweenManager() override						 = default;
	TweenManager(TweenManager&&) noexcept			 = default;
	TweenManager& operator=(TweenManager&&) noexcept = default;
	TweenManager(const TweenManager&)				 = delete;
	TweenManager& operator=(const TweenManager&)	 = delete;

	/*
	 * Load a uniquely identifiable tween into the manager.
	 * If the tween key already exists, does nothing.
	 * @param key Unique id of the item to be loaded.
	 * @return Reference to the loaded item.
	 */
	template <typename TKey>
	Item& Load(const TKey& key) {
		Tween t;
		t.Create();
		return MapManager<Tween>::Load(key, std::move(t));
	}

	/*
	 * Tweens without a key will be unloaded once the following two conditions are met:
	 * 1. The tween is completed.
	 * 2. The returned tween handle reaches a reference count of 1, i.e. it only exists in the tween
	 * manager. If the nameless tween already exists in the nameless list (based on equals
	 * comparison), this function does nothing.
	 * @return Reference handle to the loaded nameless tween.
	 */
	[[nodiscard]] Tween& Load() {
		Tween t;
		t.Create();
		return MapManagerWithNameless<Tween>::Load(std::move(t));
	}

private:
	using InternalKey = typename MapManagerWithNameless<Tween>::InternalKey;

	friend class Game;

	void Update();
};

} // namespace impl

} // namespace ptgn