#pragma once

#include <cstdlib> // std::size_t

namespace ptgn {

enum class SceneStatus : std::size_t {
	IDLE,
	DELETE
};

class Scene {
public:
	virtual ~Scene() = default;
	virtual void Enter() {}
	virtual void Update(float dt) {}
	virtual void Exit() {}
private:
	friend class SceneManager;
	SceneStatus status_{ SceneStatus::IDLE };
};

} // namespace ptgn