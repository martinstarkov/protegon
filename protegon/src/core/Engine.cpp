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

V2_int Engine::GetScreenSize() { return GetInstance().window_size_; }
int Engine::GetScreenWidth() { return GetInstance().window_size_.x; }
int Engine::GetScreenHeight() { return GetInstance().window_size_.y; }
std::size_t Engine::GetFPS() { return GetInstance().fps_; }
double Engine::GetInverseFPS() { return GetInstance().inverse_fps_; }
std::int64_t Engine::GetTimeSinceStart() const { return timer_.ElapsedMilliseconds(); }
void Engine::Delay(std::int64_t milliseconds) { SDL_Delay(static_cast<std::uint32_t>(milliseconds)); }
void Engine::Quit() { GetInstance().running_ = false; }

Window& Engine::GetWindow() {
	auto& engine{ GetInstance() };
	assert(engine.window_.IsValid() && "Cannot return uninitialized window");
	return engine.window_;
}

Renderer& Engine::GetRenderer() {
	auto& engine{ GetInstance() };
	assert(engine.renderer_.IsValid() && "Cannot return uninitialized renderer");
	return engine.renderer_;
}

std::pair<Window, Renderer> Engine::GenerateWindow(const char* title, const V2_int& position, const V2_int& size, std::uint32_t window_flags, std::uint32_t renderer_flags) {
	auto& engine{ GetInstance() };
	assert(engine.sdl_init_ == 0 && "Cannot generate window before initializing SDL");
	Window window{ title, position, size, window_flags };
	if (window.IsValid()) {
		LOG("Created window successfully");
		Renderer renderer{ window, -1, renderer_flags };
		if (renderer.IsValid()) {
			LOG("Created renderer successfully");
			return { window, renderer };
		}
		assert(!"SDL failed to create renderer");
	}
	assert(!"SDL failed to create window");
	return {};
}

Engine& Engine::GetInstance() {
	assert(instance_ != nullptr && "Engine instance could not be created properly");
	return *instance_;
}

void Engine::InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags) {
	LOG("Initializing SDL...");
	sdl_init_ = SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO);
	if (sdl_init_ == 0) {
		LOG("Initialized SDL successfully");
		auto [window, renderer] = GenerateWindow(window_title_, window_position_, window_size_, window_flags, renderer_flags);
		window_ = window;
		renderer_ = renderer;
		LOG("Initializing TTF...");
		ttf_init_ = TTF_Init();
		if (ttf_init_ == 0) {
			LOG("Initialized TTF successfully");
			LOG("All SDL components fully initialized");
			return;
		}
		assert(!"Failed to initialize true type fonts for SDL");
	}
	assert(!"SDL failed to initialize");
}

void Engine::Loop() {
	// Expected time between frames running at a certain FPS.
	const std::int64_t frame_delay{ static_cast<std::int64_t>(1000.0 * inverse_fps_) };
	while (running_) {
		auto loop_start{ timer_.ElapsedMilliseconds() };
		InputHandler::Update();

		// Update everything here.
		Update();

		renderer_.Clear();
		TextureManager::SetDrawColor(TextureManager::GetDefaultRendererColor()); // Reset renderer color.
		
		// Render everything here.
		Render();

		renderer_.Present();

		// Cap frame rate at whatever fps_ was set to.
		auto loop_time = timer_.ElapsedMilliseconds() - loop_start;
		if (loop_time < frame_delay) {
			Delay(frame_delay - loop_time);
		}
	}
}

void Engine::Clean() {
	TextureManager::Clean();
	FontManager::Clean();
	window_.Destroy();
	renderer_.Destroy();
	// Quit SDL subsystems.
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

} // namespace engine