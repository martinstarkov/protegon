#pragma once

#include "renderer/Color.h"

struct SDL_Window;

namespace ptgn {

struct SDLWindow {
	static SDLWindow& Get() {
		static SDLWindow instance;
		return instance;
	}
	Color color_;
	SDL_Window* window_{ nullptr };
};

} // namespace ptgn