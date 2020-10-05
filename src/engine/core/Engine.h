#pragma once

#include <cstdint>
#include <SDL.h>
#include <engine/ecs/ECS.h>
#include <engine/ecs/Components.h>
#include <engine/ecs/Systems.h>
#include <Defines.h>

namespace engine {

namespace internal {

#define CENTERED SDL_WINDOWPOS_CENTERED

// Default window title
constexpr const char* WINDOW_TITLE = "Unknown Title";
// Default window position centered
constexpr int WINDOW_X = CENTERED;
constexpr int WINDOW_Y = CENTERED;
// Default window width
constexpr int WINDOW_WIDTH = 600;
// Default window height
constexpr int WINDOW_HEIGHT = 480;

} // namespace internal

class BaseGame;

class Engine {
public:
	virtual void Init() {}
	virtual void Update() {}
	virtual void Render() {}
	// Default values defined in engine
	template <typename T>
	static void Start(const char* title = internal::WINDOW_TITLE, int width = internal::WINDOW_WIDTH, int height = internal::WINDOW_HEIGHT, int x = internal::WINDOW_X, int y = internal::WINDOW_Y, std::uint32_t window_flags = 0, std::uint32_t renderer_flags = 0) {
		window_title_ = title;
		window_width_ = width;
		window_height_ = height;
		window_x_ = x;
		window_y_ = x;
		InitSDL(window_flags, renderer_flags);
		running_ = true;
		Loop<T>();
		Clean();
	}
	static void Quit();
	static SDL_Window& GetWindow();
	static SDL_Renderer& GetRenderer();
	static int ScreenWidth();
	static int ScreenHeight();
protected:
	ecs::Manager manager;
private:
	static void InputUpdate();
	template <typename T>
	static Engine* GetInstance() {
		if (!instance_) {
			instance_ = new T();
		}
		return instance_;
	}
	static void Clean();
	template <typename T>
	static void Loop() {
		const std::uint32_t delay = 1000 / FPS;
		std::uint32_t start;
		std::uint32_t time;
		auto& ref = *GetInstance<T>();
		ref.Init();
		while (running_) {
			start = SDL_GetTicks();
			// Possibly pass delta T to lambda
			InputUpdate();
			ref.Update();
			SDL_RenderClear(renderer_);
			TextureManager::SetDrawColor(TextureManager::GetDefaultRendererColor());
			// Render everything here
			ref.Render();
			SDL_RenderPresent(renderer_);
			time = SDL_GetTicks() - start;
			if (delay > time) { // cap frame time at an FPS
				SDL_Delay(delay - time);
			}
		}
	}
	static void InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags);
	static SDL_Window* window_;
	static SDL_Renderer* renderer_;
	static Engine* instance_;
	static bool running_;
	static int window_width_;
	static int window_height_;
	static int window_x_;
	static int window_y_;
	static const char* window_title_;
};

} // namespace engine
