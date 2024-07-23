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

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized -> Resizing will make the viewport update during
	// resizing.
	game.event.window.Subscribe(
		WindowEvent::Resized, (void*)this,
		std::function([&](const WindowResizedEvent& e) { SetSize(e.size); })
	);

	SDL_GL_SetSwapInterval(1);
}

} // namespace ptgn