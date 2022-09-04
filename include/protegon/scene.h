#pragma once

namespace ptgn {

class Scene {
public:
	virtual ~Scene() = default;
	virtual void Update(float dt) {}
};

} // namespace ptgn