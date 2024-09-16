#pragma once

#include "scene/camera.h"

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;

	virtual void Update([[maybe_unused]] float dt) { /* user implementation */
	}

	virtual void Update() { /* user implementation */
	}

	// Called when the scene is set to active.
	virtual void Init() { /* user implementation */
	}

	// Called when the scene is removed from active scenes.
	virtual void Shutdown() { /* user implementation */
	}

	CameraManager camera;

private:
	friend class SceneManager;
	enum class Status : std::size_t {
		Idle,
		Delete
	};
	Status status_{ Status::Idle };
};

} // namespace ptgn