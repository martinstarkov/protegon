#pragma once

#include <cstdlib>

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;
	virtual void Enter() {}
	virtual void Update(double dt) {}
	std::size_t GetId() const { return id_; }
private:
	friend class SceneManager;
	bool destroy_{ false };
	std::size_t id_{ 0 };
};

} // namespace ptgn