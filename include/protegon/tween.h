#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "core/manager.h"
#include "protegon/math.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/time.h"

namespace ptgn {

class TweenManager;
class Game;

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

using TweenDestroyCallback = std::function<void()>;

namespace impl {

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
	~TweenInstance();

	// Value between [0.0f, 1.0f] indicating how much of the total duration the tween has passed in
	// the current repetition. Note: This value remains 0.0f to 1.0f even when the tween is reversed
	// or yoyoing.
	float progress_{ 0.0f };

	std::size_t index_{ 0 };
	std::vector<TweenPoint> tweens_points_;

	TweenDestroyCallback on_destroy_;
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
	explicit Tween(milliseconds duration);

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
	Tween& OnDestroy(const TweenDestroyCallback& callback);
	Tween& OnReset(const TweenCallback& callback);

	[[nodiscard]] float GetProgress() const;
	[[nodiscard]] std::int64_t GetRepeats() const;

	[[nodiscard]] bool IsCompleted() const;
	[[nodiscard]] bool IsStarted() const;
	[[nodiscard]] bool IsPaused() const;

	// TODO: Implement and test.
	// dt in seconds.
	// float Rewind(float dt) {
	// return Step(-dt);
	//}

	// dt in seconds.
	float Step(float dt);
	float Seek(float new_progress);
	float Seek(milliseconds time);

	Tween& Start();
	Tween& Pause();
	Tween& Resume();
	// Will trigger OnStop callback if tween was started or completed.
	Tween& Reset();
	Tween& Stop();
	Tween& Complete();
	Tween& Forward();
	Tween& Backward();

	// Clears previously assigned tween points.
	Tween& Clear();

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

	Tween& SetDuration(milliseconds duration, std::size_t tween_point_index = 0);

private:
	[[nodiscard]] float SeekImpl(float new_progress);
	[[nodiscard]] float StepImpl(float dt, bool accumulate_progress);
	[[nodiscard]] float AccumulateProgress(float new_progress);

	void ActivateCallback(const TweenCallback& callback);
	void PointCompleted();
	void HandleCallbacks(bool suppress_update);

	float UpdateImpl(bool suppress_update = false);
};

namespace impl {

class TweenManager : public Manager<Tween> {
public:
	using Manager::Manager;

	template <typename TKey>
	void KeepAlive(const TKey& key) {
		auto k{ GetInternalKey(key) };
		keep_alive_tweens_.insert(k);
	}

	template <typename TKey>
	void Unload(const TKey& key) {
		auto k{ GetInternalKey(key) };
		Manager::Unload(k);
		keep_alive_tweens_.erase(k);
	}

	void Clear();
	void Reset();

	void Update();

private:
	std::unordered_set<InternalKey> keep_alive_tweens_;
};

} // namespace impl

} // namespace ptgn