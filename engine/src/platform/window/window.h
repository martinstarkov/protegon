#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "serialization/json/enum.h"

struct SDL_Window;

namespace ptgn {

// TODO: Make it so these can be | together.
enum class WindowSetting {
	None,
	Windowed,
	Fullscreen,
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

namespace gl {

class GLContext;

} // namespace gl

}; // namespace impl

struct WindowConfig {
	std::string title{ "Default Title" };

	V2_int size{ 800, 800 };

	bool resizeable{ true };

	/// @brief X position of the window on the screen. If nullopt, defaults to centered.
	std::optional<int> x;

	/// @brief Y position of the window on the screen. If nullopt, defaults to centered.
	std::optional<int> y;

	bool minimized{ false };
	bool maximized{ false };
	bool fullscreen{ false };

	/// @brief If true, the window will start with the mouse captured.
	bool mouse_grabbed{ false };

	/// @brief If true, the window will always be above other windows.
	bool always_on_top{ false };

	/// @brief If true, the window will have no border or title bar. This is not the same as
	/// fullscreen, as a borderless window can still be resized and moved around.
	bool borderless{ false };

	/// @brief If true window will be transparent in the areas with alpha of 0.
	bool transparent{ false };
};

std::ostream& operator<<(std::ostream& os, const WindowConfig& config);

class Window {
public:
	Window() = delete;
	explicit Window(const WindowConfig& config);
	~Window() noexcept					 = default;
	Window(Window&&) noexcept			 = delete;
	Window& operator=(Window&&) noexcept = delete;
	Window(const Window&)				 = delete;
	Window& operator=(const Window&)	 = delete;

	void SetMinimumSize(V2_int minimum_size) const;
	[[nodiscard]] V2_int GetMinimumSize() const;

	void SetMaximumSize(V2_int maximum_size) const;
	[[nodiscard]] V2_int GetMaximumSize() const;

	void SetSize(V2_int new_size, bool centered = true) const;
	[[nodiscard]] V2_int GetSize() const;

	// @return Top left of the window relative to the top left of the screen.
	[[nodiscard]] V2_int GetPosition() const;

	void SetTitle(const std::string& title) const;
	[[nodiscard]] std::string_view GetTitle() const;

	void Center() const;

	void SetBackgroundColor(Color background_color = color::Transparent);
	[[nodiscard]] Color GetBackgroundColor() const;

	void SetPosition(V2_int new_origin) const;

	void SetSetting(WindowSetting setting) const;

	// Get the current state of a window setting.
	[[nodiscard]] bool GetSetting(WindowSetting setting) const;

	void SetResizable() const;

	void SetFixedSize() const;

	void SetFullscreen() const;

	void SwapBuffers() const;

#ifdef __EMSCRIPTEN__
	void SetCanvasSize(V2_int new_size) const;
	[[nodiscard]] V2_int GetCanvasSize() const;
#endif

	// TODO: Move to private.
	operator SDL_Window*() const;

private:
	friend class impl::gl::GLContext;

	// While the mouse is in relative mode, the cursor is hidden, the mouse position is constrained
	// to the window, and there will be continuous relative mouse motion events triggered even if
	// the mouse is at the edge of the window.
	// @param Whether or not mouse relative mode should be turned on or not.
	void SetRelativeMouseMode(bool on) const;

	void SetMouseGrab(bool on) const;
	void CaptureMouse(bool on) const;
	void SetAlwaysOnTop(bool on) const;

	Color background_color_{ color::Transparent };
	std::unique_ptr<SDL_Window, impl::WindowDeleter> instance_;
};

PTGN_SERIALIZE_ENUM(
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