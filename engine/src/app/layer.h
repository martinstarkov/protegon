#pragma once

namespace ptgn {

class Layer {
public:
	virtual ~Layer() = default;

	virtual void OnUpdate() { /* User implementation */ }

	virtual void OnRender() { /* User implementation */ }
};

} // namespace ptgn