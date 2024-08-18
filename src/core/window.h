#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"

struct SDL_Window;

namespace ptgn {

enum class FullscreenMode {
	Windowed		  = 0,
	Fullscreen		  = 1,
	DesktopFullscreen = 4096
};

class InputHandler;

namespace impl {

class GLContext;

struct WindowDeleter {
	void operator()(SDL_Window* window);
};

} // namespace impl

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

class Window {
private:
	Window();
	~Window()						 = default;
	Window(const Window&)			 = delete;
	Window(Window&&)				 = default;
	Window& operator=(const Window&) = delete;
	Window& operator=(Window&&)		 = default;

public:
	void SetMinimumSize(const V2_int& minimum_size);
	[[nodiscard]] V2_int GetMinimumSize();

	void SetSize(const V2_int& new_size, bool centered = true);
	[[nodiscard]] V2_int GetSize();

	// Returns the center coordinate of the window.
	[[nodiscard]] V2_float GetCenter();

	[[nodiscard]] V2_int GetPosition();

	void SetTitle(const char* new_title);
	[[nodiscard]] const char* GetTitle();

	void Center();

	void SetPosition(const V2_int& new_origin);

	void SetFullscreen(FullscreenMode mode);

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
	friend class impl::GLContext;
	friend class Game;

	void SetRelativeMouseMode(bool on);
	void SetMouseGrab(bool on);
	void CaptureMouse(bool on);
	void SetAlwaysOnTop(bool on);

	[[nodiscard]] bool Exists();

	std::unique_ptr<SDL_Window, impl::WindowDeleter> window_;
};

} // namespace ptgn