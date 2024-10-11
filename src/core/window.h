#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "protegon/vector2.h"
#include "utility/handle.h"

struct SDL_Window;

namespace ptgn {

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

enum class WindowSetting {
	Windowed,
	Fullscreen, /* borderless fullscreen window (desktop fullscreen) */
	Borderless,
	Bordered,
	// Note: The Maximized and Minimized settings are cancelled by setting Resizeable.
	Resizable,
	FixedSize,
	Maximized,
	Minimized,
	Shown,
	Hidden
};

namespace impl {

class Renderer;
class InputHandler;
class GLContext;

struct WindowDeleter {
	void operator()(SDL_Window* window) const;
};

struct WindowInstance {
	WindowInstance();
	std::unique_ptr<SDL_Window, WindowDeleter> window_;
	operator SDL_Window*() const;
};

class Window : public Handle<WindowInstance> {
public:
	Window()						 = default;
	~Window() override				 = default;
	Window(const Window&)			 = delete;
	Window(Window&&)				 = default;
	Window& operator=(const Window&) = delete;
	Window& operator=(Window&&)		 = default;

	void SetMinimumSize(const V2_int& minimum_size) const;
	[[nodiscard]] V2_int GetMinimumSize() const;

	void SetMaximumSize(const V2_int& maximum_size) const;
	[[nodiscard]] V2_int GetMaximumSize() const;

	void SetSize(const V2_int& new_size, bool centered = true) const;
	[[nodiscard]] V2_int GetSize() const;

#ifdef __EMSCRIPTEN__
	void SetCanvasSize(const V2_int& new_size) const;
	[[nodiscard]] V2_int GetCanvasSize() const;
#endif

	// Returns the center coordinate of the window.
	[[nodiscard]] V2_float GetCenter() const;

	[[nodiscard]] V2_int GetPosition() const;

	void SetTitle(const std::string& title) const;
	[[nodiscard]] std::string_view GetTitle() const;

	void Center() const;

	void SetPosition(const V2_int& new_origin) const;

	void SetSetting(WindowSetting setting) const;

	// Get the current state of a window setting.
	[[nodiscard]] bool GetSetting(WindowSetting setting) const;

private:
	friend class Game;
	friend class GLContext;
	friend class Renderer;

	void* CreateGLContext();
	int MakeGLContextCurrent(void* context);

	void SwapBuffers() const;

	void Init();
	void Shutdown();

	void SetRelativeMouseMode(bool on) const;
	void SetMouseGrab(bool on) const;
	void CaptureMouse(bool on) const;
	void SetAlwaysOnTop(bool on) const;
};

} // namespace impl

} // namespace ptgn