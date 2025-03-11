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

class StreamWriter;
class StreamReader;

enum class TweenEase {
	Linear,
	InSine,
	OutSine,
	InOutSine,
	// InQuad,
	// OutQuad,
	// InOutQuad,
	// InCubic,
	// OutCubic,
	// InOutCubic,
	// InExponential,
	// OutExponential,
	// InOutExponential,
	// InCircular,
	// OutCircular,
	// InOutCircular,
	//  TODO: Add custom easing function support
};

class Tween;

using TweenCallback = std::variant<
	std::function<void(Tween&, float)>, std::function<void(Tween&)>, std::function<void(float)>,
	std::function<void()>>;

namespace impl {

class Game;

using TweenEaseFunction = std::function<float(float, float, float)>;

[[nodiscard]] TweenEaseFunction GetEaseFunction(TweenEase V);

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
	TweenEase easing_func_{ TweenEase::Linear };

	TweenCallback on_complete_;
	TweenCallback on_repeat_;
	TweenCallback on_yoyo_;
	TweenCallback on_start_;
	TweenCallback on_stop_;
	TweenCallback on_update_;
	TweenCallback on_pause_;
	TweenCallback on_resume_;

	static void Serialize(ptgn::StreamWriter* w, const TweenPoint& tween_point);
	static void Deserialize(ptgn::StreamReader* r, TweenPoint& tween_point);
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

	// @param duration The time it takes to take progress from 0 to 1, or vice versa for reversed
	// tweens. Yoyo tweens take twice the duration to complete a full
	// yoyo cycle.
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
	// @param force If true, ignores the current state of the tween. If false, will only start if
	// the tween is paused or not currently started.
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

	static void Serialize(StreamWriter* w, const Tween& tween);
	static void Deserialize(StreamReader* w, Tween& tween);

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

	bool paused_{ false };
	bool started_{ false };

	TweenCallback on_reset_;
};

} // namespace ptgn