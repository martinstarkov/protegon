#pragma once

#include <functional>
#include <unordered_map>
#include <variant>

#include "protegon/math.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/time.h"

namespace ptgn {

class TweenManager;

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

using TweenEaseFunction = std::function<float(float, float, float)>;

const static std::unordered_map<TweenEase, TweenEaseFunction> tween_ease_functions_{
	{ TweenEase::Linear,
	  [](float t, float a, float b) -> float {
		  float c{ b - a };
		  return a + t * c;
	  } },
	{ TweenEase::InSine,
	  [](float t, float a, float b) -> float {
		  float c{ b - a };
		  return -c * std::cos(t * half_pi<float>) + b;
	  } },
	{ TweenEase::OutSine,
	  [](float t, float a, float b) -> float {
		  float c{ b - a };
		  return c * std::sin(t * half_pi<float>) + a;
	  } },
	{ TweenEase::InOutSine,
	  [](float t, float a, float b) -> float {
		  float c{ b - a };
		  return -c / float{ 2 } * (std::cos(pi<float> * t) - float{ 1 }) + a;
	  } },
	//{ TweenEase::InQuad,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutQuad,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutQuad,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InCubic,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutCubic,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutCubic,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InExponential,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutExponential,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutExponential,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InCircular,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutCircular,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutCircular,
	//  [](float t, float a, float b) -> float {
	//	  return a; // TODO: Implement.
	//  } },
};

struct TweenPoint {
	TweenPoint(milliseconds duration) : duration_{ duration } {}

	milliseconds duration_{ 0 };

	TweenEaseFunction easing_func_{
		tween_ease_functions_.find(TweenEase::Linear)->second
	};								   // easing function between tween start and end value.

	std::int64_t current_repeat_{ 0 }; // current number of repetitions of the tween.

	std::int64_t total_repeats_{
		0
	};						 // total number of repetitions of the tween (-1 for infinite tween).

	bool yoyo_{ false };	 // go back and fourth between values (requires repeat != 0) (both
							 // directions take duration time).
	bool reversed_{ false }; // start reversed.

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
	std::size_t index_{ 0 };
	std::vector<TweenPoint> tweens_points_;

	~TweenInstance();

	impl::TweenPoint& GetCurrentTweenPoint();
	const impl::TweenPoint& GetCurrentTweenPoint() const;
	impl::TweenPoint& GetLastTweenPoint();

	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	bool paused_{ false };
	bool started_{ false };
};

} // namespace impl

class Tween : public Handle<impl::TweenInstance> {
public:
	Tween() = default;
	Tween(milliseconds duration);

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

	[[nodiscard]] float GetProgress() const;
	[[nodiscard]] std::int64_t GetRepeats() const;

	[[nodiscard]] bool IsCompleted() const;
	[[nodiscard]] bool IsStarted() const;
	[[nodiscard]] bool IsPaused() const;

	[[nodiscard]] bool IsValid() const;

	// TODO: Implement and test.
	// dt in seconds.
	// float Rewind(float dt);

	// dt in seconds.
	float Step(float dt);
	float Seek(float new_progress);
	float Seek(milliseconds time);

	Tween& Start();
	Tween& Pause();
	void Resume();
	void Reset();
	void Stop();
	void Destroy();
	void Complete();
	void Forward();
	void Backward();

	// Clears previously assigned tween points.
	void Clear();

	template <typename Duration = milliseconds>
	[[nodiscard]] Duration GetDuration(std::size_t tween_point_index = 0) const {
		PTGN_ASSERT(IsValid(), "Cannot get duration of uninitialized or destroyed tween");
		PTGN_ASSERT(
			tween_point_index < instance_->tweens_points_.size(),
			"Specified tween point index is out of range. Ensure tween points has been added "
			"beforehand"
		);
		return std::chrono::duration_cast<Duration>(
			instance_->tweens_points_[tween_point_index].duration_
		);
	}

	void SetDuration(milliseconds duration, std::size_t tween_point_index = 0);

private:
	friend class TweenManager;
	friend struct impl::TweenInstance;

	void PointCompleted();

	float GetNewProgress(duration<float> time) const;
	float SeekImpl(float new_progress);
	float StepImpl(float dt, bool accumulate_progress);
	float AccumulateProgress(float new_progress);

	void ActivateCallback(const TweenCallback& callback, float value);

	void HandleCallbacks(bool suppress_update);
	float UpdateImpl(bool suppress_update = false);
};

} // namespace ptgn