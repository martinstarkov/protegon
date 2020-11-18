#include "Engine.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <cassert>

#include "renderer/FontManager.h"
#include "renderer/TextureManager.h"
#include "event/InputHandler.h"

namespace engine {

Engine* Engine::instance_{ nullptr };
Window Engine::window_{ nullptr };
Renderer Engine::renderer_{ nullptr };
bool Engine::running_{ false };
V2_int Engine::window_size_{ 0, 0 };
V2_int Engine::window_position_{ 0, 0 };
int Engine::sdl_init{ 1 };
int Engine::ttf_init{ 1 };
const char* Engine::window_title_{ "" };

void Engine::InputHandlerUpdate() {
	engine::InputHandler::Update();
}

void Engine::ResetWindowColor() {
	engine::TextureManager::SetDrawColor(engine::TextureManager::GetDefaultRendererColor());
}

void Engine::InitInternals() {
	engine::InputHandler::Init();
}

std::pair<Window, Renderer> Engine::GenerateWindow(const char* window_title, V2_int window_position, V2_int window_size, std::uint32_t window_flags, std::uint32_t renderer_flags) {
	assert(sdl_init == 0 && "Cannot generate window before initializing SDL");
	auto window = SDL_CreateWindow(window_title, window_position.x, window_position.y, window_size.x, window_size.y, window_flags);
	if (window) {
		LOG("Initialized window successfully");
		auto renderer = SDL_CreateRenderer(window, -1, renderer_flags);
		if (renderer) {
			LOG("Initialized renderer successfully");
			return { window, renderer };
		} else {
			assert(!"SDL failed to create renderer");
		}
	} else {
		assert(!"SDL failed to create window");
	}
	assert(false && "Cannot return null window and renderer");
	return {};
}

void Engine::InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags) {
	sdl_init = SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO);
	if (sdl_init == 0) {
		LOG("Initialized SDL successfully");
		auto [window, renderer] = GenerateWindow(window_title_, window_position_, window_size_, window_flags, renderer_flags);
		window_ = window;
		renderer_ = renderer;
		ttf_init = TTF_Init();
		if (ttf_init == 0) { // True type fonts.
			LOG("Initialized true type fonts successfully");
			// SDL fully initialized.
			return;
		} else {
			assert(!"SDL failed to initialize true type fonts");
		}
	} else {
		assert(!"SDL failed to initialize");
	}
}

void Engine::Clean() {
	engine::TextureManager::Clean();
	engine::FontManager::Clean();
	window_.Destroy();
	renderer_.Destroy();
	// Quit SDL subsystems
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

std::uint32_t Engine::GetTicks() {
	return SDL_GetTicks();
}

void Engine::Delay(std::uint32_t milliseconds) {
	SDL_Delay(milliseconds);
}

} // namespace engine