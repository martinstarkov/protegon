#include "Engine.h"

#include <cassert>
#include <Vec2D.h>
#include <SDL_image.h>
#include <engine/renderer/TextureManager.h>
#include <engine/event/InputHandler.h>

namespace engine {

Engine* Engine::instance_{ nullptr };
SDL_Window* Engine::window_{ nullptr };
SDL_Renderer* Engine::renderer_{ nullptr };
bool Engine::running_{ false };
// Defined in Init()
int Engine::window_width_{ -1 };
int Engine::window_height_{ -1 };
int Engine::window_x_{ -1 };
int Engine::window_y_{ -1 };
int Engine::frame_rate_{ -1 };
const char* Engine::window_title_{ "" };

SDL_Window& Engine::GetWindow() { assert(window_ != nullptr); return *window_; }
SDL_Renderer& Engine::GetRenderer() { assert(renderer_ != nullptr); return *renderer_; }
int Engine::ScreenWidth() { return window_width_; }
int Engine::ScreenHeight() { return window_height_; }
int Engine::FPS() { return frame_rate_; }

void Engine::InputUpdate() {
	engine::InputHandler::Update();
}

void Engine::InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags) {
	if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
		window_ = SDL_CreateWindow(window_title_, window_x_, window_y_, window_width_, window_height_, window_flags);
		if (window_) {
			renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);
			if (renderer_) {
				// SDL fully initialized
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
	SDL_DestroyWindow(window_);
	SDL_DestroyRenderer(renderer_);
	// Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Engine::Quit() { running_ = false; }

} // namespace engine