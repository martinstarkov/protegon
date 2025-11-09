#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "math/vector2.h"
#include "serialization/json/enum.h"

struct SDL_Window;

namespace ptgn {

struct Screen {
	[[nodiscard]] static V2_int GetSize();
};

// TODO: Make it so these can be | together.
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

struct WindowDeleter {
	void operator()(SDL_Window* window) const;
};

}; // namespace impl

class Window {
public:
	Window() = delete;
	// TODO: Add flags to window constructor.
	explicit Window(const char* title, const V2_int& size);
	~Window() noexcept					 = default;
	Window(Window&&) noexcept			 = delete;
	Window& operator=(Window&&) noexcept = delete;
	Window(const Window&)				 = delete;
	Window& operator=(const Window&)	 = delete;

	void SetMinimumSize(const V2_int& minimum_size) const;
	[[nodiscard]] V2_int GetMinimumSize() const;

	void SetMaximumSize(const V2_int& maximum_size) const;
	[[nodiscard]] V2_int GetMaximumSize() const;

	void SetSize(const V2_int& new_size, bool centered = true) const;
	[[nodiscard]] V2_int GetSize() const;

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

#ifdef __EMSCRIPTEN__
	void SetCanvasSize(const V2_int& new_size) const;
	[[nodiscard]] V2_int GetCanvasSize() const;
#endif

private:
	operator SDL_Window*() const;

	void SetRelativeMouseMode(bool on) const;
	void SetMouseGrab(bool on) const;
	void CaptureMouse(bool on) const;
	void SetAlwaysOnTop(bool on) const;

	std::unique_ptr<SDL_Window, impl::WindowDeleter> instance_;
};

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