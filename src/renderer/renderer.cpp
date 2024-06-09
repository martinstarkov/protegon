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

void DrawTexture(
	const Texture& texture,
	const Rectangle<int>& destination_rect,
	const Rectangle<int>& source_rect,
	float angle,
	Flip flip,
	V2_int* center_of_rotation
) {
	PTGN_CHECK(texture.IsValid(), "Cannot draw texture which is uninitialized or destroyed to renderer");
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_RenderCopyEx(
		renderer.get(),
		texture.GetInstance().get(),
		source_rect.IsEmpty() ? NULL : &SDL_Rect(source_rect),
		destination_rect.IsEmpty() ? NULL : &SDL_Rect(destination_rect),
		angle,
		center_of_rotation == nullptr ? NULL : &SDL_Point(*center_of_rotation),
		static_cast<SDL_RendererFlip>(flip)
	);
}

} // namespace renderer

} // namespace ptgn