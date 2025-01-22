#pragma once

#include <functional>
#include <memory>
#include <set>

#include "ecs/ecs.h"
#include "renderer/color.h"
#include "utility/time.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class SceneManager;

} // namespace impl

enum class TransitionType {
	None,
	Custom,
	Fade,
	FadeThroughColor,
	PushLeft,
	PushRight,
	PushUp,
	PushDown,
	UncoverLeft,
	UncoverRight,
	UncoverUp,
	UncoverDown,
	CoverLeft,
	CoverRight,
	CoverUp,
	CoverDown
};

class SceneTransition {
public:
	SceneTransition() = default;
	SceneTransition(TransitionType type, milliseconds duration);

	bool operator==(const SceneTransition& o) const;
	bool operator!=(const SceneTransition& o) const;

	SceneTransition& SetType(TransitionType type);

	// For TransitionType::FadeThroughColor, this is the time outside of the fade color
	// i.e. total transition duration = duration + color duration
	SceneTransition& SetDuration(milliseconds duration);

	// Only applies when using TransitionType::FadeThroughColor.
	SceneTransition& SetFadeColor(const Color& color);

	// The amount of time spent purely in in the fade color.
	// Only applies when using TransitionType::FadeThroughColor.
	SceneTransition& SetFadeColorDuration(milliseconds duration);

	// Custom transition callbacks.

	// float is fraction from 0 to 1 of the duration.
	std::function<void(float)> update_in;
	std::function<void()> start_in;
	std::function<void()> stop_in;

	// float is fraction from 0 to 1 of the duration.
	std::function<void(float)> update_out;
	std::function<void()> start_out;
	std::function<void()> stop_out;

private:
	friend class impl::SceneManager;

	void Start(const std::shared_ptr<Scene>& scene) const;

	Color fade_color_{ color::Black };

	// The amount of time spent purely in fade color.
	// Only applies when using TransitionType::FadeThroughColor.
	milliseconds color_duration_{ 500 };

	TransitionType type_{ TransitionType::None };

	milliseconds duration_{ 0 };
};

class Scene {
public:
	Scene();
	virtual ~Scene() = default;

	// Called when the scene is set as active.
	virtual void Enter() {
		/* user implementation */
	}

	// Called once per frame when this scene is active.
	virtual void Update() {
		/* user implementation */
	}

	// Called when the scene is no longer active.
	virtual void Exit() {
		/* user implementation */
	}

	ecs::Manager manager;

private:
	friend class impl::SceneManager;
	friend class SceneTransition;

	enum class Action {
		Enter,
		Unload
	};

	void Add(Action new_action);
	void Remove(Action action);

	// @return True if the scene has uncompleted actions, false otherwise.
	[[nodiscard]] bool HasActions() const;

	std::set<Action> actions_;
};

} // namespace ptgn