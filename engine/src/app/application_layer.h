#pragma once

namespace ptgn {

class Application;

class ApplicationLayer {
public:
	virtual ~ApplicationLayer() = default;

	virtual void OnUpdate() { /* User implementation */ }

	virtual void OnRender() { /* User implementation */ }

protected:
	friend class Application;

	bool update_enabled_{ true };
	bool render_enabled_{ true };
};

} // namespace ptgn