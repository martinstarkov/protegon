#pragma once

#include <set>

#include "ecs/ecs.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class SceneManager;

} // namespace impl

class Scene {
public:
	Scene()			 = default;
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