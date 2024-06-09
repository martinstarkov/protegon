#include "protegon/renderer.h"

#include <SDL.h>

#include "core/game.h"
#include "protegon/texture.h"

namespace ptgn {

namespace renderer {

void SetDrawColor(const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
}

void ResetDrawColor() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
}

void SetBlendMode(BlendMode mode) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawBlendMode(renderer.get(), static_cast<SDL_BlendMode>(mode));
}

void SetDrawMode(const Color& color, BlendMode mode) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawBlendMode(renderer.get(), static_cast<SDL_BlendMode>(mode));
	SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
}

void Clear() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_RenderClear(renderer.get());
}

void Present() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_RenderPresent(renderer.get());
}

void SetTarget(const Texture& texture) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderTarget(renderer.get(), texture.GetInstance().get());
}

void ResetTarget() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderTarget(renderer.get(), NULL);
}

} // namespace renderer

} // namespace ptgn