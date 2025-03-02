#pragma once

#include <set>

#include "ecs/ecs.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "renderer/render_target.h"
#include "scene/camera.h"

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

	[[nodiscard]] V2_float GetMousePosition() const;

	// void ClearTarget();
	void InternalLoad();
	void InternalUnload();
	void InternalEnter();
	void InternalUpdate();
	void InternalExit();

	void Add(Action new_action);
};

} // namespace ptgn