#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "core/util/handle.h"
#include "math/vector2.h"
#include "serialization/json/enum.h"

struct SDL_Window;

namespace ptgn {

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

enum class WindowSetting {
	None,
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
	WindowInstance(WindowInstance&&) noexcept			 = default;
	WindowInstance& operator=(WindowInstance&&) noexcept = default;
	WindowInstance(const WindowInstance&)				 = delete;
	WindowInstance& operator=(const WindowInstance&)	 = delete;
	~WindowInstance()									 = default;
	std::unique_ptr<SDL_Window, WindowDeleter> window_;
	operator SDL_Window*() const;
};

class Window : public Handle<WindowInstance> {
public:
	Window()							 = default;
	Window(Window&&) noexcept			 = default;
	Window& operator=(Window&&) noexcept = default;
	Window(const Window&)				 = delete;
	Window& operator=(const Window&)	 = delete;
	~Window()							 = default;

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

	// @return Top left of the window relative to the top left of the screen.
	[[nodiscard]] V2_int GetPosition() const;

	void SetTitle(const std::string& title) const;
	[[nodiscard]] std::string_view GetTitle() const;

	void Center() const;

	void SetPosition(const V2_int& new_origin) const;

	void SetSetting(WindowSetting setting) const;

	// Get the current state of a window setting.
	[[nodiscard]] bool GetSetting(WindowSetting setting) const;

	void SetResizable() const;

	void SetFixedSize() const;

	void SetFullscreen() const;

	void SwapBuffers() const;

private:
	friend class Game;
	friend class GLContext;
	friend class Renderer;

	void* CreateGLContext();
	int MakeGLContextCurrent(void* context);

	void Init();
	void Shutdown();

	void SetRelativeMouseMode(bool on) const;
	void SetMouseGrab(bool on) const;
	void CaptureMouse(bool on) const;
	void SetAlwaysOnTop(bool on) const;
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	WindowSetting, { { WindowSetting::None, "none" },
					 { WindowSetting::Windowed, "windowed" },
					 { WindowSetting::Fullscreen, "fullscreen" },
					 { WindowSetting::Borderless, "borderless" },
					 { WindowSetting::Bordered, "bordered" },
					 { WindowSetting::Resizable, "resizable" },
					 { WindowSetting::FixedSize, "fixed_size" },
					 { WindowSetting::Maximized, "maximized" },
					 { WindowSetting::Minimized, "minimized" },
					 { WindowSetting::Shown, "shown" },
					 { WindowSetting::Hidden, "hidden" } }
);

} // namespace ptgn