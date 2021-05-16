#include "Engine.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <cassert>

#include "renderer/Color.h"
#include "renderer/text/FontManager.h"
#include "renderer/TextureManager.h"
#include "event/InputHandler.h"

namespace engine {

Engine* Engine::instance_{ nullptr };

void Engine::Delay(milliseconds time) {
	SDL_Delay(static_cast<std::uint32_t>(time.count()));
}

void Engine::Quit() {
	GetInstance().running_ = false;
}

Display Engine::GetDisplay(std::size_t display_index) {
	auto& engine{ GetInstance() };
	if (display_index == 0) {
		display_index = engine.primary_display_index_;
	}
	auto it{ engine.display_map_.find(display_index) };
	assert(it != engine.display_map_.end() && "Cannot retrieve display with the given display index");
	assert(it->second.first.IsValid() && "Cannot retrieve display with destroyed window");
	assert(it->second.second.IsValid() && "Cannot retrieve display with destroyed renderer");
	return it->second;
}

void Engine::Quit(const Window& window) {
	auto& engine{ GetInstance() };
	for (auto it{ engine.display_map_.begin() }; it != engine.display_map_.end();) {
		if (it->second.first.window_ == window) {
			it->second.second.Destroy(); // Destroy renderer.
			it->second.first.Destroy(); // Destroy window.
			if (it->first == engine.primary_display_index_) {
				Quit();
				break;
			} else {
				engine.display_map_.erase(it++); // Erase display from engine.
			}
		} else {
			++it;
		}
	}
}

std::size_t Engine::GetPrimaryDisplayIndex() {
	return GetInstance().primary_display_index_;
}

Display Engine::CreateDisplay(const char* window_title,
							  const V2_int& window_position,
							  const V2_int& window_size,
							  std::uint32_t window_flags, 
							  std::uint32_t renderer_flags) {
	auto& engine{ GetInstance() };
	assert(engine.init_ && "Cannot generate window before initializing SDL components");
	auto display_index{ ++engine.next_display_index_ };
	Window window{ 
		window_title, 
		window_position, 
		window_size, 
		display_index, 
		window_flags 
	};
	if (window.IsValid()) {
		// Display index is stored into the Renderer via the Window object (in Renderer constructor).
		Renderer renderer{ window, -1, renderer_flags };
		if (renderer.IsValid()) {
			auto pair{ 
				engine.display_map_.emplace(display_index, Display{ window, renderer }) 
			};
			assert(pair.second && "Could not emplace window-renderer display pair into engine");
			return pair.first->second;
		}
		assert(!"SDL failed to create renderer");
	}
	assert(!"SDL failed to create window");
	return {};
}

void Engine::DestroyDisplay(const Window& window) {
	auto& engine{ GetInstance() };
	engine.display_map_.erase(window.GetDisplayIndex());
}

void Engine::DestroyDisplay(const Renderer& renderer) {
	auto& engine{ GetInstance() };
	engine.display_map_.erase(renderer.GetDisplayIndex());
}

void Engine::DestroyDisplay(std::size_t display_index) {
	auto& engine{ GetInstance() };
	if (display_index == 0) {
		display_index = engine.primary_display_index_;
	}
	engine.display_map_.erase(display_index);
}

void Engine::SetDefaultDisplay(const Window& window) {
	auto& engine{ GetInstance() };
	engine.primary_display_index_ = window.GetDisplayIndex();
}

void Engine::SetDefaultDisplay(const Renderer& renderer) {
	auto& engine{ GetInstance() };
	engine.primary_display_index_ = renderer.GetDisplayIndex();
}

void Engine::SetDefaultDisplay(std::size_t display_index) {
	auto& engine{ GetInstance() };
	if (display_index == 0) {
		display_index = engine.primary_display_index_;
	}
	engine.primary_display_index_ = display_index;
}

Engine& Engine::GetInstance() {
	assert(instance_ != nullptr && "Engine instance could not be created properly");
	return *instance_;
}

void Engine::InitSDLComponents() {
	assert(!init_ && "Cannot initialize SDL components more than one time");
	auto sdl_init{ SDL_Init(SDL_INIT_AUDIO |
							SDL_INIT_EVENTS |
							SDL_INIT_TIMER |
							SDL_INIT_VIDEO) };
	if (sdl_init != 0) {
		std::cerr << "SDL_Init: " << SDL_GetError() << std::endl;
		exit(2);
	}
	auto img_flags{
		IMG_INIT_PNG |
		IMG_INIT_JPG
	};
	auto img_init{ IMG_Init(img_flags) };
	if ((img_init & img_flags) != img_flags) {
		std::cerr << "IMG_Init: Failed to init required png and jpg support!" << std::endl;
		std::cerr << "IMG_Init: " << IMG_GetError() << std::endl;
		exit(3);
	}
	auto ttf_init{ TTF_Init() };
	if (ttf_init == -1) {
		std::cerr << "TTF_Init: " << TTF_GetError() << std::endl;
		exit(4);
	}
	init_ = true;
	//LOG("Successfully initialized all SDL components!");
}

void Engine::InitInternals() {
	InputHandler::Init();
}

void Engine::Loop() {
	// Expected time between frames running at a certain FPS.
	const milliseconds frame_delay{ math::Round<std::int64_t>(1000.0 / fps_) };
	while (running_) {
		auto loop_start{ timer_.Elapsed<milliseconds>() };

		InputHandler::Update();

		if (!running_) break;

		// Update everything here.
		Update();

		if (!running_) break;

		for (auto& display : display_map_) {
			display.second.second.SetDrawColor(colors::DEFAULT_BACKGROUND_COLOR);
		}

		for (auto& display : display_map_) {
			display.second.second.Clear();
		}

		for (auto& display : display_map_) {
			display.second.second.SetDrawColor(colors::DEFAULT_DRAW_COLOR);
		}

		// Render everything here.
		Render();

		for (auto& display : display_map_) {
			display.second.second.Present();
		}

		// Cap frame rate at whatever fps_ was set to.
		auto loop_time{ timer_.Elapsed<milliseconds>() - loop_start };
		if (loop_time < frame_delay) {
			Delay(frame_delay - loop_time);
		}
	}
	Print("Primary window closed. Exiting program...");
}

void Engine::Clean() {
	TextureManager::Clear();
	FontManager::Clear();

	for (auto& pair : display_map_) {
		pair.second.second.Destroy(); // Destroy renderer.
		pair.second.first.Destroy(); // Destroy window.
	}
	display_map_.clear();

	primary_display_index_ = 0;
	next_display_index_ = 0;

	// Quit SDL subsystems.
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
	init_ = false;
}

} // namespace engine