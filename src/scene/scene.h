#pragma once

#include <set>

#include "ecs/ecs.h"
#include "physics/physics.h"
#include "scene/camera.h"

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

	// Called when the scene is added to active scenes.
	virtual void Enter() {
		/* user implementation */
	}

	// Called once per frame for each active scene.
	virtual void Update() {
		/* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void Exit() {
		/* user implementation */
	}

	ecs::Manager manager;
	// SceneInput input;
	impl::Physics physics;
	impl::CameraManager camera;

private:
	friend class impl::SceneManager;
	friend class SceneTransition;

	void InternalEnter();
	void InternalUpdate();
	void InternalExit();

	std::size_t key_;

	bool active_{ false };

	// If the actions is manually numbered, its order determines the execution order of scene
	// functions.
	enum class Action {
		Enter  = 0,
		Exit   = 1,
		Unload = 2
	};

	std::set<Action> actions_;

	void Add(Action new_action);
};

} // namespace ptgn