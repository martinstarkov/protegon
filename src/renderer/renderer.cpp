#include "renderer.h"

#include "SDL.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"

namespace ptgn {

void Renderer::SetClearColor(const Color& color) const {
	GLRenderer::SetClearColor(color);
}

void Renderer::Clear() const {
	GLRenderer::Clear();
}

void Renderer::Present() const {
	game.window.SwapBuffers();
}

void Renderer::SetSize(const V2_int& size) {
	GLRenderer::SetSize(size);
}

void Renderer::Init() {
	GLRenderer::Init();
	SetSize(game.window.GetSize());

	// TODO: Check that this works.
	game.event.window.Subscribe(WindowEvent::Resize, (void*)this, std::function([&](const WindowResizeEvent& e) {
		SetSize(e.size);
	}));

	SDL_GL_SetSwapInterval(1);
}

} // namespace ptgn