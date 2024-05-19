#include "protegon/window.h"

#include <SDL.h>

#include "core/game.h"

namespace ptgn {

namespace window {

bool Exists() {
	return global::GetGame().sdl.GetWindow() != nullptr;
}

void Clear() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot clear window with nonexistent renderer");
	SDL_RenderClear(renderer);
}

void SetLogicalSize(const V2_int& logical_size) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot set window logical size with nonexistent renderer");
	//SDL_RenderSetIntegerScale(renderer,	static_cast<SDL_bool>(true));
	SDL_RenderSetLogicalSize(renderer, logical_size.x, logical_size.y);
	// Ensure mouse position matches latest window scale.
	global::GetGame().input.ForceUpdateMousePosition();
}

V2_int GetLogicalSize() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Cannot get logical size of window with nonexistent renderer");
	V2_int logical_size;
	SDL_RenderGetLogicalSize(renderer, &logical_size.x, &logical_size.y);
	return logical_size;
}

V2_int GetSize() {
	V2_int size;
	assert(Exists() && "Cannot get size of nonexistent window");
	SDL_GetWindowSize(global::GetGame().sdl.GetWindow(), &size.x, &size.y);
	return size;
}

V2_int GetOriginPosition() {
	V2_int origin;
	assert(Exists() && "Cannot get origin position of nonexistent window");
	SDL_GetWindowPosition(global::GetGame().sdl.GetWindow(), &origin.x, &origin.y);
	return origin;
}

const char* GetTitle() {
	assert(Exists() && "Cannot get title of nonexistent window");
	return SDL_GetWindowTitle(global::GetGame().sdl.GetWindow());
}

Color GetColor() {
	assert(Exists() && "Cannot get color of nonexistent window");
	return global::GetGame().sdl.GetWindowBackgroundColor();
}

void SetSize(const V2_int& new_size, bool centered) {
	assert(Exists() && "Cannot set size of nonexistent window");
	SDL_SetWindowSize(global::GetGame().sdl.GetWindow(), new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) Center();
}

void SetPosition(const V2_int& new_origin) {
	assert(Exists() && "Cannot set origin position of nonexistent window");
	SDL_SetWindowPosition(global::GetGame().sdl.GetWindow(), new_origin.x, new_origin.y);
}

void Center() {
	SetPosition({ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void SetTitle(const char* new_title) {
	assert(Exists() && "Cannot set title of nonexistent window");
	return SDL_SetWindowTitle(global::GetGame().sdl.GetWindow(), new_title);
}

void SetFullscreen(bool on) {
	assert(Exists() && "Cannot toggle nonexistent window fullscreen");
	if (on)
		SDL_SetWindowFullscreen(global::GetGame().sdl.GetWindow(), SDL_WINDOW_FULLSCREEN_DESKTOP);
	else
		SDL_SetWindowFullscreen(global::GetGame().sdl.GetWindow(), 0); // windowed mode.
}

void SetResizeable(bool on) {
	assert(Exists() && "Cannot toggle nonexistent window resizeability");
	SDL_SetWindowResizable(global::GetGame().sdl.GetWindow(), static_cast<SDL_bool>(on));
}

void SetColor(const Color& new_color) {
	global::GetGame().sdl.SetWindowBackgroundColor(new_color);
}

void Maximize() {
	assert(Exists() && "Cannot maximize nonexistent window");
	SDL_MaximizeWindow(global::GetGame().sdl.GetWindow());
}

void Minimize() {
	assert(Exists() && "Cannot minimize nonexistent window");
	SDL_MinimizeWindow(global::GetGame().sdl.GetWindow());
}

void Show() {
	assert(Exists() && "Cannot show nonexistent window");
	SDL_ShowWindow(global::GetGame().sdl.GetWindow());
}

void Hide() {
	assert(Exists() && "Cannot hide nonexistent window");
	SDL_HideWindow(global::GetGame().sdl.GetWindow());
}

void SetScale(const V2_float& new_scale) {
	auto& game{ global::GetGame() };
	game.sdl.SetScale(new_scale);
	// Ensure mouse position matches latest window scale.
	game.input.ForceUpdateMousePosition();
}

V2_float GetScale() {
	auto& sdl{ global::GetGame().sdl };
	return sdl.GetScale();
}

} // namespace window

} // namespace ptgn