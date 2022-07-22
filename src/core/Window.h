#pragma once

#include "renderer/Colors.h"

struct SDL_Window;

namespace ptgn {

struct Window {
	static Window& Get() {
		static Window instance;
		return instance;
	}
	Color color_{ color::WHITE };
	SDL_Window* window_{ nullptr };
	V2_double scale_{ 1.0, 1.0 };
};

} // namespace ptgn