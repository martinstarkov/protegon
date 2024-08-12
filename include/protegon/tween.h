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

struct TweenConfig {
	TweenEase ease{ TweenEase::Linear };
	milliseconds delay{ 0 }; // before starting
	// TODO: Add -1 for infinite repeats.
	std::int64_t repeat{ 1 }; // number of repetitions of the tween (-1 for infinite tween)
	bool yoyo{ false };		  // whether or not to go back and fourth (both ways take duration time)
	bool reverse{ false };
	bool paused{ false };	  // whether to start it paused

	std::function<void(Tween&, TweenType)> on_complete;
	std::function<void(Tween&, TweenType)> on_repeat;
	std::function<void(Tween&, TweenType)> on_start;
	std::function<void(Tween&, TweenType)> on_stop;
	std::function<void(Tween&, TweenType)> on_update;
	std::function<void(Tween&, TweenType)> on_yoyo;
	std::function<void(Tween&, TweenType)> on_pause;
	std::function<void(Tween&, TweenType)> on_resume;

	// TODO: Implement delays.
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

	bool running_{ false };
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
		instance_			 = std::make_shared<impl::TweenInstance>();
		instance_->from_	 = from;
		instance_->to_		 = to;
		instance_->config_	 = config;
		instance_->duration_ = duration;
		if (!instance_->config_.paused) {
			Start();
		}
	}

	void Start() {
		PTGN_ASSERT(IsValid(), "Cannot start uninitialized or destroyed tween");
		PTGN_ASSERT(!instance_->running_, "Cannot start tween which has already been started");
		Reset();
		instance_->running_ = true;
		if (const auto& f = instance_->config_.on_start; f != nullptr) {
			f(*this, GetValue());
		}
	}

	void Pause() {
		PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
		if (!instance_->config_.paused) {
			instance_->config_.paused = true;
			if (const auto& f = instance_->config_.on_pause; f != nullptr) {
				f(*this, GetValue());
			}
		}
	}

	void Resume() {
		PTGN_ASSERT(IsValid(), "Cannot pause uninitialized or destroyed tween");
		if (instance_->config_.paused) {
			instance_->config_.paused = false;
			if (const auto& f = instance_->config_.on_resume; f != nullptr) {
				f(*this, GetValue());
			}
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
		if (!instance_->running_ || instance_->config_.paused) {
			return GetValue();
		}
		instance_->elapsed_ += std::chrono::round<milliseconds>(
			duration<float>(dt) * (instance_->config_.reverse ? -1 : 1)
		);
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
		if (!instance_->running_ || instance_->config_.paused) {
			return GetValue();
		}
		instance_->elapsed_ = time;
		return UpdateImpl();
	}

	TweenType GetValue() const {
		return GetValueImpl(GetProgress());
	}

	// Returns value in range [0.0f, 1.0f] depending on how much of the total tween duration has
	// elapsed.
	float GetProgress() const {
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

	TweenType GetFrom() const {
		PTGN_ASSERT(IsValid(), "Cannot get from value of uninitialized or destroyed tween");
		return instance_->from_;
	}

	TweenType GetTo() const {
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

	TweenConfig& GetConfig() {
		PTGN_ASSERT(IsValid(), "Cannot get config of uninitialized or destroyed tween");
		return instance_->config_;
	}

	const TweenConfig& GetConfig() const {
		PTGN_ASSERT(IsValid(), "Cannot get config of uninitialized or destroyed tween");
		return instance_->config_;
	}

	bool IsRunning() const {
		return IsValid() && instance_->running_;
	}

	template <typename Duration = milliseconds>
	Duration GetElapsed() const {
		PTGN_ASSERT(IsValid(), "Cannot get elapsed time of uninitialized or destroyed tween");
		return std::chrono::duration_cast<Duration>(instance_->elapsed_);
	}

	template <typename Duration = milliseconds>
	Duration GetDuration() const {
		PTGN_ASSERT(IsValid(), "Cannot get duration of uninitialized or destroyed tween");
		return std::chrono::duration_cast<Duration>(instance_->duration_);
	}

	std::int64_t GetRepeats() const {
		PTGN_ASSERT(IsValid(), "Cannot get repeats of uninitialized or destroyed tween");
		return instance_->repeats_;
	}

	void SetReversed(bool reversed = true) {
		PTGN_ASSERT(IsValid(), "Cannot reverse uninitialized or destroyed tween");
		instance_->config_.reverse = reversed;
	}

	void Forward() {
		SetReversed(false);
	}

	void Backward() {
		SetReversed(true);
	}

	void Complete() {
		Seek(1.0f);
	}

	void Reset() {
		PTGN_ASSERT(IsValid(), "Cannot reset uninitialized or destroyed tween");
		instance_->repeats_ = 0;
		instance_->elapsed_ = decltype(instance_->elapsed_){ 0 };
		instance_->running_ = false;
	}

	// Stops and destroys the tween.
	void Stop() {
		PTGN_ASSERT(IsValid(), "Cannot stop uninitialized or destroyed tween");
		PTGN_ASSERT(instance_->running_, "Cannot stop tween which has not been started");
		if (const auto& f = instance_->config_.on_stop; f != nullptr) {
			f(*this, GetValue());
		}
		instance_.reset();
	}

	// TODO: Implement jump(tween point) 0, 1 or 2 or 3, etc

	// TODO: setTimeScale(0.5) - runs at half speed
private:
	TweenType GetValueImpl(float progress) const {
		PTGN_ASSERT(IsValid(), "Cannot get value of uninitialized or destroyed tween");
		auto it = impl::tween_ease_functions_.find(instance_->config_.ease);
		PTGN_ASSERT(it != impl::tween_ease_functions_.end(), "Failed to recognize easing type");
		const auto& ease_func = it->second;
		return ease_func(progress, instance_->from_, instance_->to_);
	}

	TweenType UpdateImpl() {
		auto progress{ GetProgress() };

		bool end_value{ progress >= 1.0f };

		if (end_value && instance_->repeats_ != -1) {
			instance_->repeats_++;
		}

		PTGN_ASSERT(instance_->repeats_ <= instance_->config_.repeat);

		bool completed{ instance_->repeats_ == instance_->config_.repeat && end_value };

		TweenType value{ GetValueImpl(progress) };

		if (instance_->running_ && !instance_->config_.paused) {
			if (const auto& f = instance_->config_.on_update; f != nullptr) {
				f(*this, value);
			}
			if (completed) {
				instance_->running_ = false;
				if (const auto& f = instance_->config_.on_complete; f != nullptr) {
					f(*this, value);
				}
			}
		}
		return value;
	}
};

} // namespace ptgn