#pragma once

#include <set>

#include "camera/camera.h"

namespace ptgn {

namespace impl {

class SceneManager;

} // namespace impl

class Scene {
public:
	virtual ~Scene() = default;

	// Called when the scene is initially loaded.
	virtual void Preload() {
		/* user implementation */
	}

	// Called when the scene is added to active scenes.
	virtual void Init() {
		/* user implementation */
	}

	virtual void Update() {
		/* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void Shutdown() {
		/* user implementation */
	}

	// Called when the scene is unloaded.
	virtual void Unload() {
		/* user implementation */
	}

	impl::CameraManager camera;

private:
	friend class impl::SceneManager;

	// Order determines execution order of scene functions.
	enum class Action {
		Preload	 = 0,
		Init	 = 1,
		Shutdown = 2,
		Unload	 = 3
	};

	void Add(Action new_action);

	// Each scene must be preloaded initially.

	std::set<Action> actions_{ Action::Preload };
};

} // namespace ptgn