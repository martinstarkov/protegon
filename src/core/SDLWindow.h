#pragma once

#include "renderer/Colors.h"

struct SDL_Window;

namespace ptgn {

struct SDLWindow {
	enum class Flags {
		SDL_WINDOW_HIDDEN = 8
	};
	static SDLWindow& Get() {
		static SDLWindow instance;
		return instance;
	}
	Color color_{ color::WHITE };
	SDL_Window* window_{ nullptr };
};

} // namespace ptgn