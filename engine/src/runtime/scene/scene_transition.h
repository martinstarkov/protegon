#pragma once

#include <concepts>
#include <type_traits>

#include "core/math/easing.h"
#include "core/time/time.h"

namespace ptgn {

class Scene;
class SceneManager;
class EventHandler;

class SceneTransition {
public:
	SceneTransition() = default;
	explicit SceneTransition(
		milliseconds duration, milliseconds delay = milliseconds{ 0 }, Ease ease = Ease::Linear
	);

	virtual ~SceneTransition() = default;

	/// @brief Returns the fraction of the transition that has elapsed, taking into account the
	/// easing.
	float GetElapsedFraction() const;

	/// @brief Returns the fraction of the transition that has elapsed, without taking into account
	/// the easing.
	float GetUneasedElapsedFraction() const;

	milliseconds GetDuration() const;
	Ease GetEase() const;

	virtual void OnDelayStart([[maybe_unused]] Scene& target_scene
	) { /* Optional user implementation */ }

	virtual void OnStart([[maybe_unused]] Scene& target_scene) { /* Optional user implementation */
	}

	virtual void OnUpdate([[maybe_unused]] Scene& target_scene) { /* Optional user implementation */
	}

	virtual void OnStop([[maybe_unused]] Scene& target_scene) { /* Optional user implementation */ }

private:
	friend class SceneManager;
	friend class EventHandler;

	void UpdateTime(secondsf dt);
	void UpdateDelayTime(secondsf dt);
	[[nodiscard]] bool IsFinished() const;
	[[nodiscard]] bool IsInDelay() const;

	milliseconds elapsed_{ 0 };
	milliseconds duration_{ 0 };

	milliseconds delay_elapsed_{ 0 };
	milliseconds delay_duration_{ 0 };
	bool started_{ false };

	Ease ease_{ Ease::Linear };
};

struct NoTransition {};

template <typename T>
concept SceneTransitionType =
	std::derived_from<T, SceneTransition> || std::same_as<std::decay_t<T>, NoTransition>;

template <
	SceneTransitionType TransitionOut = NoTransition,
	SceneTransitionType TransitionIn  = NoTransition>
struct SceneTransitionPair {
	SceneTransitionPair() = default;

	SceneTransitionPair(TransitionOut&& out, TransitionIn&& in) :
		out{ std::move(out) }, in{ std::move(in) } {}

	TransitionOut out{};
	TransitionIn in{};
};

} // namespace ptgn