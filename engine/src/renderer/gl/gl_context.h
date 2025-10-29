#pragma once

struct SDL_Window;

namespace ptgn::impl::gl {

// Must be constructed after SDL_Window has been created.
class GLContext {
public:
	GLContext() = delete;
	explicit GLContext(SDL_Window* window);
	~GLContext() noexcept;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

private:
	static void LoadGLFunctions();

	void* context_{ nullptr };
};

} // namespace ptgn::impl::gl