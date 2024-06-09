#include "protegon/window.h"

#include <SDL.h>

#include "core/game.h"
#include "protegon/input.h"

namespace ptgn {

namespace screen {

V2_int GetSize() {
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
		SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		PTGN_ASSERT(!"Failed to retrieve screen size");
	}
	return V2_int{ dm.w, dm.h };
}

} // namespace screen

namespace window {

void SetupSize(const V2_int& resolution, const V2_int& minimum_resolution, bool fullscreen, bool borderless, bool resizeable, const V2_float& scale) {
	window::SetMinimumSize(minimum_resolution);
	window::SetFullscreen(fullscreen);
	window::SetScale(scale);
	window::SetResolution(resolution);

	if (!fullscreen) {
		window::SetBorderless(borderless);
		if (borderless) {
			window::SetSize(screen::GetSize());
		} else {
			window::SetSize(resolution);
			window::SetResizeable(resizeable);
		}
	}
}

void SetScale(const V2_float& scale) {
	auto& sdl{ global::GetGame().sdl };
	sdl.SetScale(scale);
	// Ensure mouse position matches latest window scale.
	global::GetGame().input.ForceUpdateMousePosition();
}

V2_float GetScale() {
	PTGN_ASSERT(Exists(), "Cannot get scale of nonexistent window");
	return global::GetGame().sdl.GetScale();
}

bool Exists() {
	return global::GetGame().sdl.WindowExists();
}

void Clear() {
	SDL_RenderClear(global::GetGame().sdl.GetRenderer().get());
}

void SetResolution(const V2_int& resolution) {
	auto& sdl{ global::GetGame().sdl };
	sdl.SetResolution(resolution);
	//SDL_RenderSetIntegerScale(renderer,	static_cast<SDL_bool>(integer_scaling));
	SDL_RenderSetLogicalSize(sdl.GetRenderer().get(), resolution.x, resolution.y);
	// Ensure mouse position matches latest window scale.
	global::GetGame().input.ForceUpdateMousePosition();
}

void SetMinimumSize(const V2_int& minimum_size) {
	PTGN_ASSERT(Exists(), "Cannot set minimum size of nonexistent window");
	SDL_SetWindowMinimumSize(global::GetGame().sdl.GetWindow().get(), minimum_size.x, minimum_size.y);
}

V2_int GetMinimumSize() {
	PTGN_ASSERT(Exists(), "Cannot get minimum size of nonexistent window");
	V2_int minimum_size;
	SDL_GetWindowMinimumSize(global::GetGame().sdl.GetWindow().get(), &minimum_size.x, &minimum_size.y);
	return minimum_size;
}

V2_int GetResolution() {
	V2_int logical_size;
	SDL_RenderGetLogicalSize(global::GetGame().sdl.GetRenderer().get(), &logical_size.x, &logical_size.y);
	return logical_size;
}

V2_int GetSize() {
	PTGN_ASSERT(Exists(), "Cannot get size of nonexistent window");
	V2_int size;
	SDL_GL_GetDrawableSize(global::GetGame().sdl.GetWindow().get(), &size.x, &size.y);
	return size;
}

V2_int GetOriginPosition() {
	PTGN_ASSERT(Exists(), "Cannot get origin position of nonexistent window");
	V2_int origin;
	SDL_GetWindowPosition(global::GetGame().sdl.GetWindow().get(), &origin.x, &origin.y);
	return origin;
}

const char* GetTitle() {
	PTGN_ASSERT(Exists(), "Cannot get title of nonexistent window");
	return SDL_GetWindowTitle(global::GetGame().sdl.GetWindow().get());
}

Color GetColor() {
	PTGN_ASSERT(Exists(), "Cannot get color of nonexistent window");
	return global::GetGame().sdl.GetWindowBackgroundColor();
}

void SetSize(const V2_int& new_size, bool centered) {
	PTGN_ASSERT(Exists(), "Cannot set size of nonexistent window");
	SDL_SetWindowSize(global::GetGame().sdl.GetWindow().get(), new_size.x, new_size.y);
	// Important to center after resizing.
	if (centered) Center();
}

void SetPosition(const V2_int& new_origin) {
	PTGN_ASSERT(Exists(), "Cannot set origin position of nonexistent window");
	SDL_SetWindowPosition(global::GetGame().sdl.GetWindow().get(), new_origin.x, new_origin.y);
}

void Center() {
	SetPosition({ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED });
}

void SetTitle(const char* new_title) {
	PTGN_ASSERT(Exists(), "Cannot set title of nonexistent window");
	return SDL_SetWindowTitle(global::GetGame().sdl.GetWindow().get(), new_title);
}

void SetFullscreen(bool on) {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window fullscreen");
	if (on)
		SDL_SetWindowFullscreen(global::GetGame().sdl.GetWindow().get(), SDL_WINDOW_FULLSCREEN_DESKTOP);
	else
		SDL_SetWindowFullscreen(global::GetGame().sdl.GetWindow().get(), 0); // windowed mode.
}

void SetResizeable(bool on) {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window resizeability");
	SDL_SetWindowResizable(global::GetGame().sdl.GetWindow().get(), static_cast<SDL_bool>(on));
}

void SetBorderless(bool on) {
	PTGN_ASSERT(Exists(), "Cannot toggle nonexistent window bordered");
	SDL_SetWindowBordered(global::GetGame().sdl.GetWindow().get(), static_cast<SDL_bool>(!on));
}

void SetColor(const Color& new_color) {
	PTGN_ASSERT(Exists(), "Cannot set color of nonexistent window");
	global::GetGame().sdl.SetWindowBackgroundColor(new_color);
}

void Maximize() {
	PTGN_ASSERT(Exists(), "Cannot maximize nonexistent window");
	auto& sdl{ global::GetGame().sdl };
	SDL_MaximizeWindow(sdl.GetWindow().get());
	// TODO: Check if this is necessary.
	sdl.SetScale(sdl.GetScale());
}

void Minimize() {
	PTGN_ASSERT(Exists(), "Cannot minimize nonexistent window");
	SDL_MinimizeWindow(global::GetGame().sdl.GetWindow().get());
}

void Show() {
	PTGN_ASSERT(Exists(), "Cannot show nonexistent window");
	SDL_ShowWindow(global::GetGame().sdl.GetWindow().get());
}

void Hide() {
	PTGN_ASSERT(Exists(), "Cannot hide nonexistent window");
	SDL_HideWindow(global::GetGame().sdl.GetWindow().get());
}

void RepeatUntilQuit(std::function<void()> while_not_quit) {
	Game& game = global::GetGame();

	bool running = true;

	game.event.window_event.Subscribe((void*)&running, [&](const Event<WindowEvent>& event) {
		if (event.Type() == WindowEvent::QUIT) {
			running = false;
		}
	});

	while (running) {
		input::Update();
		while_not_quit();
	}

	game.event.window_event.Unsubscribe((void*)&running);
}

} // namespace window

} // namespace ptgn