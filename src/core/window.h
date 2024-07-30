#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"

struct SDL_Window;

namespace ptgn {

class InputHandler;

namespace impl {

class GameInstance;

struct WindowDeleter {
	void operator()(SDL_Window* window);
};

} // namespace impl

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

class Window {
private:
	Window()						 = default;
	~Window()						 = default;
	Window(const Window&)			 = delete;
	Window(Window&&)				 = default;
	Window& operator=(const Window&) = delete;
	Window& operator=(Window&&)		 = default;
	// Setting fullscreen to true invalidates borderless and resizeable.
	// Setting borderless to true invalidates resizeable.
	void SetupSize(
		const V2_int& resolution, const V2_int& minimum_resolution, bool fullscreen = false,
		bool borderless = false, bool resizeable = true, const V2_float& scale = { 1.0f, 1.0f }
	);

	[[nodiscard]] bool Exists();

public:
	void SetMinimumSize(const V2_int& minimum_size);
	[[nodiscard]] V2_int GetMinimumSize();

	void SetSize(const V2_int& new_size, bool centered = true);
	[[nodiscard]] V2_int GetSize();

	[[nodiscard]] V2_int GetOriginPosition();

	void SetTitle(const char* new_title);
	[[nodiscard]] const char* GetTitle();

	void Center();

	void SetPosition(const V2_int& new_origin);

	void SetFullscreen(bool on);

	// Note: The effect of Maximimize() is cancelled after calling
	// SetResizeable(true).
	void SetResizeable(bool on);

	void SetBorderless(bool on);

	// Note: The effect of Maximimize() is cancelled after calling
	// SetResizeable(true).
	void Maximize();

	void Minimize();

	void Show();

	void Hide();

	// TODO: Move to private?
	void SwapBuffers();

	// TODO: Move to private
	SDL_Window* GetSDLWindow() {
		return window_.get();
	}

private:
	friend class impl::GameInstance;
	friend class Game;
	void Init();

	std::unique_ptr<SDL_Window, impl::WindowDeleter> window_;
};

} // namespace ptgn