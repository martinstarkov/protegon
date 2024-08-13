#pragma once

#include <functional>
#include <unordered_map>

#include "protegon/math.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/time.h"

namespace ptgn {

enum class TweenEase {
	Linear,
	InSine,
	OutSine,
	InOutSine,
	InQuad,
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
	InOutCircular,
	// TODO: Implement Custom ease
};

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
	microseconds duration_;
	microseconds elapsed_{ 0 };

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
	{ TweenEase::InQuad,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::OutQuad,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InOutQuad,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InCubic,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::OutCubic,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InOutCubic,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InExponential,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::OutExponential,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InOutExponential,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InCircular,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::OutCircular,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
	{ TweenEase::InOutCircular,
	  [](float t, TweenType a, TweenType b) -> TweenType {
		  return a; // TODO: Implement.
	  } },
};

} // namespace impl

class Tween : public Handle<impl::TweenInstance> {
public:
	Tween() = default;

	Tween(TweenType from, TweenType to, milliseconds duration, const TweenConfig& config = {}) {
		instance_		   = std::make_shared<impl::TweenInstance>();
		instance_->from_   = from;
		instance_->to_	   = to;
		instance_->config_ = config;
		if (instance_->config_.yoyo) {
			PTGN_ASSERT(instance_->config_.repeat != 0, "Yoyoing a tween requires repeat != 0");
		}
		instance_->duration_ = duration;
		instance_->reversed_ = instance_->config_.reversed;
		instance_->paused_	 = instance_->config_.paused;
		if (!instance_->paused_) {
			Start();
		}
	}

	void Start() {
		PTGN_ASSERT(IsValid(), "Cannot start uninitialized or destroyed tween");
		Reset();
		instance_->running_ = true;
		ActivateCallback(instance_->config_.on_start, GetValue());
	}

	void Pause() {
		PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
		if (!instance_->paused_) {
			instance_->paused_ = true;
			ActivateCallback(instance_->config_.on_pause, GetValue());
		}
	}

	void Resume() {
		PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
		if (instance_->paused_) {
			instance_->paused_ = false;
			ActivateCallback(instance_->config_.on_resume, GetValue());
		}
	}

	/*
	// TODO: Implement custome easing functions.
	template <typename T>
	Tween& SetEasingFunction(std::function<T(float p, T begin, T end)>& easing_function) {
		// TODO: for linear easing of integer types, use rounded floating points
		easing_function
	}
	*/

	// dt in seconds.
	TweenType Rewind(float dt) {
		Step(-dt);
	}

	// dt in seconds.
	TweenType Step(float dt) {
		PTGN_ASSERT(IsValid(), "Cannot update uninitialized or destroyed tween");
		if (!instance_->running_ || instance_->paused_) {
			return GetValue();
		}

		auto time{ duration<float>(dt) };

		auto add_time = [&](duration<float> t) {
			instance_->elapsed_ +=
				std::chrono::round<milliseconds>(t * (instance_->reversed_ ? -1 : 1));
		};

		if (time >= instance_->duration_) {
			auto diff{ instance_->duration_ - instance_->elapsed_ };
			PTGN_ASSERT(diff >= decltype(instance_->duration_){ 0 });
			add_time(diff);
			time -= diff;
			UpdateImpl(false);
		}

		while (time >= instance_->duration_) {
			add_time(instance_->duration_);
			time -= instance_->duration_;
			UpdateImpl(false);
		}

		add_time(time);
		return UpdateImpl();
	}

	TweenType Seek(float fraction) {
		return Seek(std::chrono::round<milliseconds>(fraction * instance_->duration_));
	}

	TweenType Seek(milliseconds time) {
		PTGN_ASSERT(IsValid(), "Cannot seek uninitialized or destroyed tween");
		PTGN_ASSERT(
			time >= milliseconds{ 0 } && time <= instance_->duration_,
			"Cannot seek outside the range of tween duration"
		);
		if (!instance_->running_ || instance_->paused_) {
			return GetValue();
		}
		instance_->elapsed_ = time;
		return UpdateImpl();
	}

	[[nodiscard]] TweenType GetValue() const {
		return GetValueImpl(GetProgress());
	}

	// Returns value in range [0.0f, 1.0f] depending on how much of the total tween duration has
	// elapsed.
	[[nodiscard]] float GetProgress() const {
		PTGN_ASSERT(IsValid(), "Cannot get progress of uninitialized or destroyed tween");
		duration<float, decltype(instance_->duration_)::period> duration_progress{
			std::chrono::duration_cast<duration<float, decltype(instance_->duration_)::period>>(
				instance_->elapsed_
			) /
			std::chrono::duration_cast<duration<float, decltype(instance_->duration_)::period>>(
				instance_->duration_
			)
		};
		// TODO: This clamp probably leaks some time and instead you should bounce the time back
		// (when yoyoing or repeating).
		float progress{ std::clamp(duration_progress.count(), 0.0f, 1.0f) };
		PTGN_ASSERT(progress >= 0.0f && progress <= 1.0f);
		return progress;
	}

	[[nodiscard]] TweenType GetFrom() const {
		PTGN_ASSERT(IsValid(), "Cannot get from value of uninitialized or destroyed tween");
		return instance_->from_;
	}

	[[nodiscard]] TweenType GetTo() const {
		PTGN_ASSERT(IsValid(), "Cannot get to value of uninitialized or destroyed tween");
		return instance_->to_;
	}

	void SetFrom(TweenType from) {
		PTGN_ASSERT(IsValid(), "Cannot set from value of uninitialized or destroyed tween");
		instance_->from_ = from;
		UpdateImpl();
	}

	void SetTo(TweenType to) {
		PTGN_ASSERT(IsValid(), "Cannot set to value of uninitialized or destroyed tween");
		instance_->to_ = to;
		UpdateImpl();
	}

	[[nodiscard]] const TweenConfig& GetConfig() const {
		PTGN_ASSERT(IsValid(), "Cannot get config of uninitialized or destroyed tween");
		return instance_->config_;
	}

	[[nodiscard]] bool IsCompleted() const {
		return IsValid() && instance_->completed_;
	}

	[[nodiscard]] bool IsRunning() const {
		return IsValid() && instance_->running_;
	}

	template <typename Duration = milliseconds>
	[[nodiscard]] Duration GetElapsed() const {
		PTGN_ASSERT(IsValid(), "Cannot get elapsed time of uninitialized or destroyed tween");
		return std::chrono::duration_cast<Duration>(instance_->elapsed_);
	}

	template <typename Duration = milliseconds>
	[[nodiscard]] Duration GetDuration() const {
		PTGN_ASSERT(IsValid(), "Cannot get duration of uninitialized or destroyed tween");
		return std::chrono::duration_cast<Duration>(instance_->duration_);
	}

	[[nodiscard]] std::int64_t GetRepeats() const {
		PTGN_ASSERT(IsValid(), "Cannot get repeats of uninitialized or destroyed tween");
		return instance_->repeats_;
	}

	void SetReversed(bool reversed = true) {
		PTGN_ASSERT(IsValid(), "Cannot reverse uninitialized or destroyed tween");
		instance_->reversed_ = reversed;
	}

	void Forward() {
		SetReversed(false);
	}

	void Backward() {
		SetReversed(true);
	}

	void Complete() {
		Seek(instance_->reversed_ ? 0.0f : 1.0f);
	}

	void Reset() {
		PTGN_ASSERT(IsValid(), "Cannot reset uninitialized or destroyed tween");
		instance_->repeats_ = 0;
		instance_->elapsed_ =
			instance_->reversed_ ? instance_->duration_ : decltype(instance_->elapsed_){ 0 };
		instance_->running_	  = false;
		instance_->completed_ = false;
	}

	void Stop() {
		PTGN_ASSERT(IsValid(), "Cannot stop uninitialized or destroyed tween");
		if (instance_->running_) {
			ActivateCallback(instance_->config_.on_stop, GetValue());
			instance_->running_ = false;
			// TODO: Consider destroying tween instance.
			// Destroy();
		}
	}

	void Destroy() {
		instance_.reset();
	}

	// TODO: Implement jump(tween point) 0, 1 or 2 or 3, etc

	// TODO: setTimeScale(0.5) - runs at half speed
private:
	void ActivateCallback(const TweenCallback& callback, TweenType value) {
		if (callback != nullptr) {
			callback(*this, value);
		}
	}

	[[nodiscard]] TweenType GetValueImpl(float progress) const {
		PTGN_ASSERT(IsValid(), "Cannot get value of uninitialized or destroyed tween");
		auto it = impl::tween_ease_functions_.find(instance_->config_.ease);
		PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Failed to recognize easing type");
		const auto& ease_func = it->second;
		return ease_func(progress, instance_->from_, instance_->to_);
	}

	TweenType UpdateImpl(bool call_update = true) {
		auto progress{ GetProgress() };

		bool infinite_loop{ instance_->config_.repeat == -1 };
		bool repeated{ instance_->repeats_ > 0 };

		bool end_value{ progress >= 1.0f && !instance_->reversed_ ||
						progress <= 0.0f && instance_->reversed_ };

		bool completed{ instance_->repeats_ == instance_->config_.repeat };

		if (end_value &&
			(!completed && instance_->repeats_ <= instance_->config_.repeat || infinite_loop)) {
			instance_->repeats_++;
		}

		PTGN_ASSERT(instance_->repeats_ <= instance_->config_.repeat || infinite_loop);

		auto value{ GetValueImpl(progress) };

		if (instance_->running_ && !instance_->paused_) {
			if (call_update) {
				ActivateCallback(instance_->config_.on_update, value);
			}
			if (end_value) {
				if (completed) {
					instance_->running_	  = false;
					instance_->completed_ = true;
					ActivateCallback(instance_->config_.on_complete, value);
				} else {
					if (instance_->config_.yoyo) {
						SetReversed(!instance_->reversed_);
						ActivateCallback(instance_->config_.on_yoyo, value);
					} else {
						instance_->elapsed_ = instance_->reversed_
												? instance_->duration_
												: decltype(instance_->elapsed_){ 0 };
					}
					ActivateCallback(instance_->config_.on_repeat, value);
				}
			}
		}

		return value;
	}
};

} // namespace ptgn