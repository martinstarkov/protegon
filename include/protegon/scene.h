#pragma once

namespace ptgn {

class Scene {
public:
	Scene() = default;
	virtual ~Scene() = default;
	virtual void Update(float dt) {}
};

} // namespace ptgn