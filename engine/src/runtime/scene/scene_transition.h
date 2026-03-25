#pragma once

#include <concepts>
#include <initializer_list>
#include <type_traits>

#include "core/time/time.h"

namespace ptgn {

class Scene;
class SceneManager;

class SceneTransition {
public:
	SceneTransition() = default;
	explicit SceneTransition(milliseconds duration);

	virtual ~SceneTransition() = default;

	float GetElapsedFraction() const;
	milliseconds GetDuration() const;

	virtual void OnStart([[maybe_unused]] Scene& target_scene) { /* Optional user implementation */
	}

	virtual void OnUpdate([[maybe_unused]] Scene& target_scene) { /* Optional user implementation */
	}

	virtual void OnStop([[maybe_unused]] Scene& target_scene) { /* Optional user implementation */ }

private:
	friend class SceneManager;

	void UpdateTime(secondsf dt);
	[[nodiscard]] bool IsFinished() const;

	milliseconds elapsed_{ 0 };
	milliseconds duration_{ 0 };
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