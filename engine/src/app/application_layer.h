#pragma once

namespace ptgn {

class ApplicationLayer {
public:
	virtual ~ApplicationLayer() = default;

	virtual void OnUpdate() { /* User implementation */ }

	virtual void OnRender() { /* User implementation */ }
};

} // namespace ptgn