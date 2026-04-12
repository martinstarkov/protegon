#pragma once

namespace ptgn {

class Application;

class Layer {
public:
	virtual ~Layer() = default;

	virtual void OnUpdate(Application& app) { /* User implementation */ }

	virtual void OnRender(Application& app) { /* User implementation */ }
};

} // namespace ptgn