#pragma once

#include <functional>
#include <unordered_map>

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

// TODO: Add more tween types.
using TweenType = float;

class Tween;

using TweenCallback = std::function<void(Tween&, TweenType)>;

struct TweenConfig {
	TweenEase ease{ TweenEase::Linear }; // easing function between tween start and end value.
	std::int64_t repeat{ 0 }; // number of repetitions of the tween (-1 for infinite tween).
	bool yoyo{ false }; // go back and fourth between values (requires repeat != 0) (both directions
						// take duration time).
	bool reversed{ false }; // start reversed.
	bool paused{ false };	// start paused.

	TweenCallback on_complete;
	TweenCallback on_repeat;
	TweenCallback on_start;
	TweenCallback on_stop;
	TweenCallback on_update;
	TweenCallback on_yoyo;
	TweenCallback on_pause;
	TweenCallback on_resume;

	// TODO: Implement delays.
	// milliseconds start_delay{ 0 }; // before starting
	// milliseconds yoyo_hold_time{ 0 }; // time before continuing yoyo
	// milliseconds repeat_delay;		  // time before repeating
	// milliseconds complete_delay{ 0 }; // time before calling complete callback
};

namespace impl {

struct TweenInstance {
	TweenType from_;
	TweenType to_;

	TweenConfig config_;

	std::int64_t repeats_{ 0 };
	milliseconds duration_;
	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	bool paused_{ false };
	bool reversed_{ false };
	bool running_{ false };
	bool completed_{ false };
};

using TweenEaseFunction = std::function<TweenType(float, TweenType, TweenType)>;

const static std::unordered_map<TweenEase, TweenEaseFunction> tween_ease_functions_{
	{ TweenEase::Linear,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  TweenType c{ b - a };
		  return a + t * c;
	  } },
	{ TweenEase::InSine,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  TweenType c{ b - a };
		  return -c * std::cos(t * half_pi<float>) + b;
	  } },
	{ TweenEase::OutSine,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  TweenType c{ b - a };
		  return c * std::sin(t * half_pi<float>) + a;
	  } },
	{ TweenEase::InOutSine,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  TweenType c{ b - a };
		  return -c / TweenType{ 2 } * (std::cos(pi<float> * t) - TweenType{ 1 }) + a;
	  } },
	//{ TweenEase::InQuad,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutQuad,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutQuad,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InCubic,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutCubic,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutCubic,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InExponential,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutExponential,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutExponential,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InCircular,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::OutCircular,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
	//{ TweenEase::InOutCircular,
	//  [](float t, TweenType a, TweenType b) -> TweenType {
	//	  return a; // TODO: Implement.
	//  } },
};

} // namespace impl

enum class TweenEvent {
	Complete,
	Repeat,
	Start,
	Stop,
	Update,
	Yoyo,
	Pause,
	Resume
};

class Tween : public Handle<impl::TweenInstance> {
public:
	Tween() = default;

	Tween(TweenType from, TweenType to, milliseconds duration, const TweenConfig& config = {});

	void SetDuration(milliseconds duration);
	void SetFromValue(TweenType from);
	void SetToValue(TweenType to);

	/*
	// TODO: Implement custome easing functions.
	template <typename T>
	Tween& SetEasingFunction(std::function<T(float p, T begin, T end)>& easing_function) {
		// TODO: for linear easing of integer types, use rounded floating points
		easing_function
	}
	*/

	void SetCallback(TweenEvent event, const TweenCallback& callback);

	[[nodiscard]] TweenType GetValue() const;
	// @return Value in range [0.0f, 1.0f] depending on how much of the total tween duration has
	// elapsed.
	[[nodiscard]] float GetProgress() const;
	[[nodiscard]] TweenType GetFromValue() const;
	[[nodiscard]] TweenType GetToValue() const;
	[[nodiscard]] const TweenConfig& GetConfig() const;
	[[nodiscard]] std::int64_t GetRepeats() const;

	[[nodiscard]] bool IsCompleted() const;
	[[nodiscard]] bool IsStarted() const;
	[[nodiscard]] bool IsPaused() const;

	// TODO: Implement and test.
	// dt in seconds.
	// TweenType Rewind(float dt);

	// dt in seconds.
	TweenType Step(float dt);
	TweenType Seek(float new_progress);
	TweenType Seek(milliseconds time);

	void Start();
	void Pause();
	void Resume();
	void Reset();
	void Stop();
	void Destroy();
	void Complete();
	void SetReversed(bool reversed = true);
	void Forward();
	void Backward();

	template <typename Duration = milliseconds>
	[[nodiscard]] Duration GetElapsed() const {
		PTGN_ASSERT(IsValid(), "Cannot get elapsed time of uninitialized or destroyed tween");
		return std::chrono::duration_cast<Duration>(GetProgress() * instance_->duration_);
	}

	template <typename Duration = milliseconds>
	[[nodiscard]] Duration GetDuration() const {
		PTGN_ASSERT(IsValid(), "Cannot get duration of uninitialized or destroyed tween");
		return std::chrono::duration_cast<Duration>(instance_->duration_);
	}

	// TODO: Implement jump(tween point) 0, 1 or 2 or 3, etc

	// TODO: setTimeScale(0.5) - runs at half speed
private:
	friend class TweenManager;

	using Time = decltype(instance_->duration_);

	float GetNewProgress(duration<float, Time::period> time) const;
	TweenType SeekImpl(float new_progress);
	TweenType StepImpl(float dt, bool accumulate_progress);
	float AccumulateProgress(float new_progress);

	void ActivateCallback(const TweenCallback& callback, TweenType value);

	void HandleCallbacks(TweenType value, bool suppress_update);
	void HandleRepeats();
	TweenType UpdateImpl(bool suppress_update = false);
};

} // namespace ptgn