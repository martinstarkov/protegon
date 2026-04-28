#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"
#include "core/util/time.h"
#include "platform/file_dialog.h"
#include "serialization/serialize.h"

struct GLFWwindow;
struct GLFWcursor;

namespace ptgn {

class SceneInput;

// TODO: Make it so these can be | together.
enum class WindowSetting {
	None,
	Windowed,
	Fullscreen,
	Borderless,
	Bordered,
	// Note: The Maximized and Minimized settings are cancelled by setting Resizable.
	Resizable,
	FixedSize,
	Maximized,
	Minimized,
	Shown,
	Hidden
};
PTGN_SERIALIZE_ENUM(WindowSetting);

namespace impl {

class ApplicationContext;

struct WindowDeleter {
	void operator()(GLFWwindow* window) const;
};

struct CursorDeleter {
	void operator()(GLFWcursor* cursor) const;
};

/// @brief Unit: seconds.
using Timestamp = double;

/// @brief Number of keys stored in the key states array.
/// From GLFW documentation.
inline constexpr std::size_t kKeyCount{ 348 };
inline constexpr std::size_t kMouseCount{ 8 };

enum class KeyState : std::uint8_t {
	Idle	 = 0, /// When the key is not pressed.
	Pressed	 = 1, /// First frame that the key is pressed.
	Held	 = 2, /// Every subsequent frame that the key is pressed.
	Released = 3, /// First frame that the key is released.
};
PTGN_SERIALIZE_ENUM(KeyState);

enum class MouseState : std::uint8_t {
	Idle	 = 0, /// When the mouse button is not pressed.
	Pressed	 = 1, /// First frame that the mouse button is pressed.
	Held	 = 2, /// Every subsequent frame that the mouse button is pressed.
	Released = 3, /// First frame that the mouse button is released.
};
PTGN_SERIALIZE_ENUM(MouseState);

namespace gl {

class GLContext;

} // namespace gl

}; // namespace impl

enum class MouseMode {
	/// @brief Visible, free
	Normal,
	/// @brief Hidden, free
	Hidden,
	/// @brief Locked to window, relative motion (FPS)
	Disabled
};
PTGN_SERIALIZE_ENUM(MouseMode);

struct WindowConfig {
	std::string title{ "Default Title" };

	V2_int size{ 800, 800 };

	bool resizable{ true };

	/// @brief X position of the window on the screen. If nullopt, defaults to centered.
	std::optional<int> x;

	/// @brief Y position of the window on the screen. If nullopt, defaults to centered.
	std::optional<int> y;

	bool minimized{ false };
	bool maximized{ false };
	bool fullscreen{ false };

	MouseMode mouse_mode{ MouseMode::Normal };

	/// @brief If true, the window will always be above other windows.
	bool always_on_top{ false };

	/// @brief If true, the window will have no border or title bar. This is not the same as
	/// fullscreen, as a borderless window can still be resized and moved around.
	bool borderless{ false };

	/// @brief If true window will be transparent in the areas with alpha of 0.
	bool transparent{ false };

	PTGN_SERIALIZE(
		WindowConfig, title, size, resizable, x, y, minimized, maximized, fullscreen, mouse_mode,
		always_on_top, borderless, transparent
	)
};

class Window {
public:
	void SetOSCursor(const path& img_filepath, V2_int cursor_hotspot = {});
	void ResetOSCursor();
	void SetOSCursorVisibility(bool visibility = true);

	void SetMinimumSize(V2_int minimum_size);
	V2_int GetMinimumSize() const;

	void SetMaximumSize(V2_int maximum_size);
	V2_int GetMaximumSize() const;

	void SetSize(V2_int new_size, bool centered = true);
	V2_int GetSize() const;

	/// @return Top left of the window relative to the top left of the screen.
	V2_int GetPosition() const;

	void SetTitle(std::string_view title);
	std::string_view GetTitle() const;

	void Center();

	void SetBackgroundColor(Color background_color = color::Transparent);
	Color GetBackgroundColor() const;

	void SetPosition(V2_int new_origin);

	void SetSetting(WindowSetting setting);

	[[nodiscard]] bool IsFocused() const;

	/// @brief Get the current state of a window setting.
	bool GetSetting(WindowSetting setting) const;

	void SetResizable();

	void SetFixedSize();

	void SetFullscreen(bool on);

	void SwapBuffers() const;

#ifdef __EMSCRIPTEN__
	void SetCanvasSize(V2_int new_size);
	V2_int GetCanvasSize() const;
#endif

	void ClearInputState();

	FileDialog file;

private:
	friend class impl::gl::GLContext;
	friend class Application;
	friend class impl::ApplicationContext;
	friend class SceneInput;
	friend class FileDialog;

	Window() = delete;
	explicit Window(
		const WindowConfig& config, std::function<void(impl::EventData&&)>&& event_sink
	);
	~Window();
	Window(Window&&) noexcept			 = delete;
	Window& operator=(Window&&) noexcept = delete;
	Window(const Window&)				 = delete;
	Window& operator=(const Window&)	 = delete;

	void SetCallbacks();

	void CacheWindowedRect();

	/// @brief While the mouse is in relative mode, the cursor is hidden, the mouse position is
	/// constrained to the window, and there will be continuous relative mouse motion events
	/// triggered even if the mouse is at the edge of the window.
	void SetMouseMode(MouseMode mode);
	void SetAlwaysOnTop(bool on);

	/// @brief Polls window events, passing them into the set event sink. Returns true if the app
	/// should stay open.
	bool PollEvents();

	std::function<void(impl::EventData&&)> event_sink_;

	/// @brief Potential custom cursor defined by the user or nullptr if the default cursor is being
	/// used.
	std::unique_ptr<GLFWcursor, impl::CursorDeleter> custom_cursor_;
	std::unique_ptr<GLFWwindow, impl::WindowDeleter> instance_;

	bool focused_{ true };

	Color background_color_{ color::Transparent };

	std::string title_;
	V2_int minimum_size_{ 1, 1 };
	V2_int maximum_size_{ -1, -1 };

	/// @brief Tracking the last known windowed rectangle.
	V2_int windowed_pos_{ 100, 100 };
	V2_int windowed_size_{ 800, 800 };
	bool windowed_was_maximized_{ false };

	/// @return Mouse position relative to the center of the window.
	V2_float GetMousePosition() const;

	/// @return Mouse position relative to the center of the window during the previous frame.
	V2_float GetPreviousMousePosition() const;

	/// @return Mouse delta (current_position - previous_position) relative to the center of the
	/// window.
	V2_float GetMouseDelta() const;

	/// @return The amount scrolled by the mouse in the current frame,
	/// positive upward right, negative downward left. Zero if no scroll occurred.
	V2_float GetMouseScroll() const;

	/// @param button The mouse button to check.
	/// @return True the first frame that the mouse is pressed.
	[[nodiscard]] bool MousePressed(Mouse button) const;

	/// @param button The mouse button to check.
	/// @return True the first frame that the mouse button is released.
	[[nodiscard]] bool MouseReleased(Mouse button) const;

	/// @param button The mouse button to check.
	/// @return True every frame that the mouse button is pressed.
	[[nodiscard]] bool MouseHeld(Mouse button) const;

	/// @param button The mouse button to check.
	/// @param time The duration of time for which the mouse should be held.
	/// @return True if the mouse button has been held for the given amount of time.
	[[nodiscard]] bool MouseHeld(Mouse button, milliseconds time) const;

	/// @param button The mouse button to check.
	/// @return The amount of time that the mouse button has been held down, negative numbers
	/// indicate the time since the mouse button was last held.
	milliseconds GetMouseHeldTime(Mouse button) const;

	/// @param key The key to check.
	/// @return True the first frame that the key is pressed.
	[[nodiscard]] bool KeyPressed(Key key) const;

	/// @param key The key to check.
	/// @return True the first frame that the key is released.
	[[nodiscard]] bool KeyReleased(Key key) const;

	/// @param key The key to check.
	/// @return True every frame that the key is pressed.
	[[nodiscard]] bool KeyHeld(Key key) const;

	/// @param key The key to check.
	/// @param time The duration of time for which the key should be held.
	/// @return True if the key has been held for the given amount of time.
	[[nodiscard]] bool KeyHeld(Key key, milliseconds time) const;

	/// @param key The key to check.
	/// @return The amount of time that the key has been held down, negative numbers
	/// indicate the time since the key was last held.
	milliseconds GetKeyHeldTime(Key key) const;

	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void PushEvent(TArgs&&... args) {
		PTGN_ASSERT(event_sink_);
		auto event{ impl::EventData::Create<T>(std::forward<TArgs>(args)...) };
		event_sink_(std::move(event));
	}

	std::array<bool, impl::kKeyCount> key_down_{};
	std::array<bool, impl::kKeyCount> prev_key_down_{};
	std::array<bool, impl::kMouseCount> mouse_down_{};
	std::array<bool, impl::kMouseCount> prev_mouse_down_{};
	std::array<impl::KeyState, impl::kKeyCount> key_states_{};
	std::array<impl::Timestamp, impl::kKeyCount> key_timestamps_{};
	std::array<impl::MouseState, impl::kMouseCount> mouse_states_{};
	std::array<impl::Timestamp, impl::kMouseCount> mouse_timestamps_{};

	/// @brief Flag that is set to true once the mouse position has been set at least once. This
	/// prevents sending the first mouse move event. Using std::optional for mouse_position_ might
	/// be a better alternative.
	bool mouse_set_{ false };

	/// @brief Raw mouse position updated by window cursor move callback. Relative to top left of
	/// the window.
	V2_float raw_mouse_position_{};

	/// @brief Current mouse position relative to the center of the window.
	V2_float mouse_position_{};

	/// @brief Previous mouse position relative to the center of the window.
	V2_float previous_mouse_position_{};

	/// @brief Raw mouse scroll accumulated in the current frame.
	V2_float raw_scroll_accum_{};

	/// @brief Current scroll amount in the current frame (not cumulative).
	V2_float mouse_scroll_{};

	/// @brief Timestamp of the most recent scroll event.
	impl::Timestamp mouse_scroll_timestamp_{ 0 };

	/// @brief Set by window quit callback.
	bool quit_{ false };
};

} // namespace ptgn