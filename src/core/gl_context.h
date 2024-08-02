#pragma once

namespace ptgn {

class Game;

namespace impl {

struct GLVersion {
	GLVersion();

	int major{ 0 };
	int minor{ 0 };
};

// Must be constructed after SDL_Window has been created.
class GLContext {
private:
	GLContext();
	~GLContext();
	GLContext(const GLContext&)			   = delete;
	GLContext(GLContext&&)				   = default;
	GLContext& operator=(const GLContext&) = delete;
	GLContext& operator=(GLContext&&)	   = default;

private:
	friend class Game;

	static void LoadGLFunctions();

	void* context_{ nullptr };
};

} // namespace impl

} // namespace ptgn