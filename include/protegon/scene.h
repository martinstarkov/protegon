#pragma once

#include "scene/camera.h"

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;

	virtual void Update([[maybe_unused]] float dt) {}

	virtual void Update() {}

	// Called when the scene is set to active.
	virtual void Init() {}

	// Called when the scene is removed from active scenes.
	virtual void Shutdown() {}

	CameraManager camera;

	/*template <typename T>
	std::shared_ptr<T> Cast() {
		return std::static_pointer_cast<T>(this);
	}*/
private:
	friend class SceneManager;
	enum class Status : std::size_t {
		Idle,
		Delete
	};
	Status status_{ Status::Idle };
};

} // namespace ptgn