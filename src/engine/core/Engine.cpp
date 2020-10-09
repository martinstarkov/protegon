#include "Engine.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert>

#include <engine/renderer/TextureManager.h>
#include <engine/event/InputHandler.h>

namespace engine {

Engine* Engine::instance_{ nullptr };
Window Engine::window_{ nullptr };
Renderer Engine::renderer_{ nullptr };
bool Engine::running_{ false };
V2_int Engine::window_size_{ 0, 0 };
V2_int Engine::window_position_{ 0, 0 };
const char* Engine::window_title_{ "" };

void Engine::InputUpdate() {
	engine::InputHandler::Update();
}

void Engine::InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags) {
	if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
		window_ = SDL_CreateWindow(window_title_, window_position_.x, window_position_.y, window_size_.x, window_size_.y, window_flags);
		if (window_) {
			renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);
			if (renderer_) {
				// SDL fully initialized
				return;
			} else {
				assert(!"SDL failed to create renderer");
			}
		} else {
			assert(!"SDL failed to create window");
		}
	} else {
		assert(!"SDL failed to initialize");
	}
}

void Engine::Clean() {
	engine::TextureManager::Clean();
	window_.Destroy();
	renderer_.Destroy();
	// Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

} // namespace engine