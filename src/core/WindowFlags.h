#pragma once

#include <cstdint> // std::uint32_t
#include "math/Vector2.h"

namespace ptgn {

namespace window {

enum class Flags : std::uint32_t {
	NONE = 0,
	FULLSCREEN = 1,            // SDL_WINDOW_FULLSCREEN
	FULLSCREEN_DESKTOP = 4097, // SDL_WINDOW_FULLSCREEN_DESKTOP
	OPENGL = 2,                // SDL_WINDOW_OPENGL
	VULKAN = 268435456,        // SDL_WINDOW_VULKAN
	SHOWN = 4,                 // SDL_WINDOW_SHOWN
	HIDDEN = 8,                // SDL_WINDOW_HIDDEN
	BORDERLESS = 16,           // SDL_WINDOW_BORDERLESS
	RESIZABLE = 32,            // SDL_WINDOW_RESIZABLE
	MINIMIZED = 64,            // SDL_WINDOW_MINIMIZED
	MAXIMIZED = 128,           // SDL_WINDOW_MAXIMIZED
	INPUT_GRABBED = 256,       // SDL_WINDOW_INPUT_GRABBED
	INPUT_FOCUS = 512,         // SDL_WINDOW_INPUT_FOCUS
	MOUSE_FOCUS = 1024,        // SDL_WINDOW_MOUSE_FOCUS
	FOREIGN = 2048,            // SDL_WINDOW_FOREIGN
	ALLOW_HIGHDPI = 8192,      // SDL_WINDOW_ALLOW_HIGHDPI
	MOUSE_CAPTURE = 16384,     // SDL_WINDOW_MOUSE_CAPTURE
	ALWAYS_ON_TOP = 32768,     // SDL_WINDOW_ALWAYS_ON_TOP
	SKIP_TASKBAR = 65536,      // SDL_WINDOW_SKIP_TASKBAR
	UTILITY = 131072,          // SDL_WINDOW_UTILITY
	TOOLTIP = 262144,          // SDL_WINDOW_TOOLTIP
	POPUP_MENU = 524288        // SDL_WINDOW_POPUP_MENU
};

// Definition for a screen position that is centered on the user's monitor.
inline constexpr V2_int CENTERED{ 805240832, 805240832 };

} // namespace window

} // namespace ptgn