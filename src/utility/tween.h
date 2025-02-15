#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "math/math.h"
#include "utility/time.h"

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
	explicit TweenPoint(milliseconds duration);

	void SetReversed(bool reversed);

	milliseconds duration_{ 0 };

	TweenEaseFunction easing_func_{
		tween_ease_functions_.find(TweenEase::Linear)->second
	}; // easing function between tween start and end value.

	// current number of repetitions of the tween.
	std::int64_t current_repeat_{ 0 };

	// total number of repetitions of the tween (-1 for infinite tween).
	std::int64_t total_repeats_{ 0 };

	// go back and fourth between values (requires repeat != 0) (both
	// directions take duration time).
	bool yoyo_{ false };

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

} // namespace impl

class Tween {
public:
	Tween()								 = default;
	Tween(const Tween& other)			 = default;
	Tween& operator=(const Tween& other) = default;
	Tween(Tween&& other) noexcept;
	Tween& operator=(Tween&& other) noexcept;
	~Tween();

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

	// @param force If true, ignores the current state of the tween. If false, will only start if
	// the tween is paused or not currently started. Resets and starts the tween. Will restart
	// paused tweens.
	Tween& Start(bool force = true);

	// If there are future tween points, will simulate a tween point completion. If the tween has
	// completed or is in the middle of the final tween point, this function does nothing.
	Tween& IncrementTweenPoint();

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

	void ActivateCallback(const TweenCallback& callback);
	void PointCompleted();
	void HandleCallbacks(bool suppress_update);

	// @return New progress of the tween after updating.
	float UpdateImpl(bool suppress_update = false);

	[[nodiscard]] float GetNewProgress(duration<float> time) const;

	[[nodiscard]] impl::TweenPoint& GetCurrentTweenPoint();
	[[nodiscard]] const impl::TweenPoint& GetCurrentTweenPoint() const;
	[[nodiscard]] impl::TweenPoint& GetLastTweenPoint();

	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	std::size_t index_{ 0 };
	std::vector<impl::TweenPoint> tween_points_;

	TweenCallback on_reset_;

	bool paused_{ false };
	bool started_{ false };
};

} // namespace ptgn