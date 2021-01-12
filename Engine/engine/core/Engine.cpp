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

void Engine::Quit() { 
	auto& engine = GetInstance();
	engine.running_ = false;
}

Window& Engine::GetWindow() {
	auto& engine = GetInstance();
	assert(engine.window_ && "Cannot return uninitialized window");
	return engine.window_;
}

Renderer& Engine::GetRenderer() {
	auto& engine = GetInstance();
	assert(engine.renderer_ && "Cannot return uninitialized renderer");
	return engine.renderer_;
}

V2_int Engine::ScreenSize() {
	auto& engine = GetInstance();
	return engine.window_size_;
}

int Engine::ScreenWidth() {
	auto& engine = GetInstance();
	return engine.window_size_.x;
}

int Engine::ScreenHeight() {
	auto& engine = GetInstance();
	return engine.window_size_.y;
}

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
	auto& engine = GetInstance();
	assert(engine.sdl_init_ == 0 && "Cannot generate window before initializing SDL");
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
	sdl_init_ = SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO);
	if (sdl_init_ == 0) {
		LOG("Initialized SDL successfully");
		auto [window, renderer] = GenerateWindow(window_title_, window_position_, window_size_, window_flags, renderer_flags);
		window_ = window;
		renderer_ = renderer;
		ttf_init_ = TTF_Init();
		if (ttf_init_ == 0) { // True type fonts.
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

std::size_t Engine::FPS() {
	auto& engine = GetInstance();
	return engine.fps_;
}

double Engine::InverseFPS() {
	auto& engine = GetInstance();
	return engine.inverse_fps_;
}

} // namespace engine