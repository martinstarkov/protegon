#pragma once

#include <set>

#include "core/entity.h"
#include "core/manager.h"
#include "physics/physics.h"
#include "scene/camera.h"
#include "scene/scene_input.h"

namespace ptgn {

class Scene;
class SceneTransition;

namespace impl {

class SceneManager;

} // namespace impl

class Scene {
private:
	// RenderTarget target_;

	std::size_t key_{ 0 };

	bool active_{ false };

	// If the actions is manually numbered, its order determines the execution order of scene
	// functions.
	enum class Action {
		Enter  = 0,
		Exit   = 1,
		Unload = 2
	};

	std::set<Action> actions_;

	Entity CreateEntity();

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

	Manager manager;
	SceneInput input;
	Physics physics;
	CameraManager camera;

private:
	friend class impl::SceneManager;
	friend class SceneTransition;

	// void ClearTarget();
	void InternalEnter();
	void PreUpdate();
	void PostUpdate();
	void InternalExit();

	void Add(Action new_action);
};

} // namespace ptgn