#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "protegon/vector2.h"

struct SDL_Window;

namespace ptgn {

enum class FullscreenMode {
	Windowed		  = 0,
	Fullscreen		  = 1,
	DesktopFullscreen = 4096
};

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

namespace impl {

class Renderer;
class InputHandler;
class GLContext;

struct WindowDeleter {
	void operator()(SDL_Window* window) const;
};

class Window {
private:
	Window()						 = default;
	~Window()						 = default;
	Window(const Window&)			 = delete;
	Window(Window&&)				 = default;
	Window& operator=(const Window&) = delete;
	Window& operator=(Window&&)		 = default;

public:
	void SetMinimumSize(const V2_int& minimum_size) const;
	[[nodiscard]] V2_int GetMinimumSize() const;

	void SetSize(const V2_int& new_size, bool centered = true) const;
	[[nodiscard]] V2_int GetSize() const;

	// Returns the center coordinate of the window.
	[[nodiscard]] V2_float GetCenter() const;

	[[nodiscard]] V2_int GetPosition() const;

	void SetTitle(const std::string& title) const;
	[[nodiscard]] std::string_view GetTitle() const;

	void Center() const;

	void SetPosition(const V2_int& new_origin) const;

	void SetFullscreen(FullscreenMode mode) const;

	// Note: The effect of Maximimize() is cancelled after calling
	// SetResizeable(true).
	void SetResizeable(bool on) const;

	void SetBorderless(bool on) const;

	// Note: The effect of Maximimize() is cancelled after calling
	// SetResizeable(true).
	void Maximize() const;
	void Minimize() const;

	void Show() const;

	void Hide() const;

private:
	friend class Game;
	friend class GLContext;
	friend class Renderer;
	friend class InputHandler;

	void SwapBuffers() const;

	void Init();
	void Shutdown();

	void SetRelativeMouseMode(bool on) const;
	void SetMouseGrab(bool on) const;
	void CaptureMouse(bool on) const;
	void SetAlwaysOnTop(bool on) const;

	[[nodiscard]] bool Exists() const;

	std::unique_ptr<SDL_Window, impl::WindowDeleter> window_;
};

} // namespace impl

} // namespace ptgn