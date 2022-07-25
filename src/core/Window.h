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
};

} // namespace ptgn